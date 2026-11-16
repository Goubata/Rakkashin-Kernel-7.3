// SPDX-License-Identifier: GPL-2.0-only
/* Actual production methods and native QMI/QRTR sockets/thermal registry.
 * The local peer records firmware replies; no physical modem is exercised. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/net.h>
#include <linux/delay.h>
#include "drivers/thermal/thermal_core.h"
#include <linux/soc/qcom/qmi.h>
static int probe_handle_init(struct qmi_handle *,size_t,const struct qmi_ops *,const struct qmi_msg_handler *);
static int probe_lookup(struct qmi_handle *,unsigned int,unsigned int,unsigned int);
static void probe_handle_release(struct qmi_handle *);
static int init_attempts,lookup_attempts,fail_init_at,fail_lookup_at,live_handles,released_handles;
static struct qmi_handle *tracked[16];
#undef module_driver
#define module_driver(driver, register, unregister)
#define qmi_handle_init probe_handle_init
#define qmi_add_lookup probe_lookup
#define qmi_handle_release probe_handle_release
#include "../../ports/qcom-qmi-cooling/qmi_cooling.c"
#undef qmi_handle_init
#undef qmi_add_lookup
#undef qmi_handle_release
static int probe_handle_init(struct qmi_handle *q,size_t len,const struct qmi_ops *ops,const struct qmi_msg_handler *h)
{
 int ret,i;
 if(++init_attempts==fail_init_at)return -ENOMEM;
 ret=qmi_handle_init(q,len,ops,h);
 if(!ret){for(i=0;i<ARRAY_SIZE(tracked);i++)if(!tracked[i]){tracked[i]=q;live_handles++;break;}}
 return ret;
}
static int probe_lookup(struct qmi_handle *q,unsigned int service,unsigned int version,unsigned int instance)
{
 if(++lookup_attempts==fail_lookup_at)return -EIO;
 return qmi_add_lookup(q,service,version,instance);
}
static void probe_handle_release(struct qmi_handle *q)
{
 int i;
 for(i=0;i<ARRAY_SIZE(tracked);i++)if(tracked[i]==q){tracked[i]=NULL;live_handles--;released_handles++;break;}
 qmi_handle_release(q);
}
#include "../../ports/qcom-qmi-cooling/thermal_mitigation_device_service_v01.c"
struct fixture {
 struct qmi_tmd_device dev;
 struct qmi_tmd_instance tmd;
 struct qmi_cooling_device client;
 struct qmi_handle server,server2;
 struct tmd_get_mitigation_device_list_resp_msg_v01 list;
 struct qmi_service service;
 atomic_t calls;
 int response_error;
 bool drop,stale,server_ready,server2_ready,driver_registered;
 struct platform_device *pdev[2],*sensors[4];
 struct thermal_zone_device *zones[4];
 int temps[4];bool map_caps;
 u8 last_state;
};
static void reply_list_common(struct fixture *f,struct qmi_handle *q,struct sockaddr_qrtr *peer,struct qmi_txn *txn)
{
 qmi_send_response(q,peer,txn,QMI_TMD_GET_MITIGATION_DEVICE_LIST_RESP_V01,
  TMD_GET_MITIGATION_DEVICE_LIST_RESP_MSG_V01_MAX_MSG_LEN,
  tmd_get_mitigation_device_list_resp_msg_v01_ei,&f->list);
}
static void reply_set_common(struct fixture *f,struct qmi_handle *q,struct sockaddr_qrtr *peer,struct qmi_txn *txn,const void *data)
{
 const struct tmd_set_mitigation_level_req_msg_v01 *req=data;
 struct tmd_set_mitigation_level_resp_msg_v01 resp={};
 f->last_state=req->mitigation_level;atomic_inc(&f->calls);
 if(READ_ONCE(f->drop))return;
 if(READ_ONCE(f->stale))thermal_qmi_del_server(&f->tmd.handle,&f->service);
 if(READ_ONCE(f->response_error)){resp.resp.result=QMI_RESULT_FAILURE_V01;resp.resp.error=1;}
 qmi_send_response(q,peer,txn,QMI_TMD_SET_MITIGATION_LEVEL_RESP_V01,
  TMD_SET_MITIGATION_LEVEL_RESP_MSG_V01_MAX_MSG_LEN,
  tmd_set_mitigation_level_resp_msg_v01_ei,&resp);
}
static void reply_list(struct qmi_handle *q,struct sockaddr_qrtr *p,struct qmi_txn *t,const void *data)
{reply_list_common(container_of(q,struct fixture,server),q,p,t);}
static void reply_set(struct qmi_handle *q,struct sockaddr_qrtr *p,struct qmi_txn *t,const void *data)
{reply_set_common(container_of(q,struct fixture,server),q,p,t,data);}
static void reply_list2(struct qmi_handle *q,struct sockaddr_qrtr *p,struct qmi_txn *t,const void *data)
{reply_list_common(container_of(q,struct fixture,server2),q,p,t);}
static void reply_set2(struct qmi_handle *q,struct sockaddr_qrtr *p,struct qmi_txn *t,const void *data)
{reply_set_common(container_of(q,struct fixture,server2),q,p,t,data);}
static const struct qmi_msg_handler handlers[]={
 {QMI_REQUEST,QMI_TMD_GET_MITIGATION_DEVICE_LIST_REQ_V01,tmd_get_mitigation_device_list_req_msg_v01_ei,sizeof(struct tmd_get_mitigation_device_list_req_msg_v01),reply_list},
 {QMI_REQUEST,QMI_TMD_SET_MITIGATION_LEVEL_REQ_V01,tmd_set_mitigation_level_req_msg_v01_ei,sizeof(struct tmd_set_mitigation_level_req_msg_v01),reply_set},{}
};
static const struct qmi_msg_handler handlers2[]={
 {QMI_REQUEST,QMI_TMD_GET_MITIGATION_DEVICE_LIST_REQ_V01,tmd_get_mitigation_device_list_req_msg_v01_ei,sizeof(struct tmd_get_mitigation_device_list_req_msg_v01),reply_list2},
 {QMI_REQUEST,QMI_TMD_SET_MITIGATION_LEVEL_REQ_V01,tmd_set_mitigation_level_req_msg_v01_ei,sizeof(struct tmd_set_mitigation_level_req_msg_v01),reply_set2},{}
};
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);int ret;
 if(!f)return -ENOMEM;test->priv=f;
 init_attempts=lookup_attempts=fail_init_at=fail_lookup_at=live_handles=released_handles=0;memset(tracked,0,sizeof(tracked));
 f->dev.instances=&f->tmd;f->dev.count=1;
 mutex_init(&f->tmd.state_mutex);mutex_init(&f->tmd.io_mutex);
 INIT_LIST_HEAD(&f->tmd.tmd_cdev_list);
 INIT_DELAYED_WORK(&f->tmd.svc_arrive_work,qmi_tmd_svc_arrive);
 f->client.tmd=&f->tmd;f->client.cdev_name="avr-qmi-private";
 strscpy(f->client.qmi_name,"modem");list_add(&f->client.qmi_node,&f->tmd.tmd_cdev_list);f->tmd.count=1;
 ret=qmi_handle_init(&f->tmd.handle,3279,&thermal_qmi_event_ops,NULL);if(ret)return ret;
 f->tmd.handle_ready=true;
 ret=qmi_handle_init(&f->server,3279,NULL,handlers);
 if(ret){qmi_tmd_cleanup(&f->dev);return ret;}f->server_ready=true;
 ret=kernel_bind(f->server.sock,(struct sockaddr_unsized *)&f->server.sq,sizeof(f->server.sq));
 if(ret){qmi_tmd_cleanup(&f->dev);qmi_handle_release(&f->server);return ret;}
 ret=kernel_getsockname(f->server.sock,(struct sockaddr *)&f->server.sq);
 if(ret<0){qmi_tmd_cleanup(&f->dev);qmi_handle_release(&f->server);return ret;}
 f->service.node=f->server.sq.sq_node;f->service.port=f->server.sq.sq_port;
 f->tmd.peer=f->server.sq;f->tmd.has_server=true;
 f->list.mitigation_device_list_valid=1;f->list.mitigation_device_list_len=1;
 strscpy(f->list.mitigation_device_list[0].mitigation_dev_id.mitigation_dev_id,"modem");
 f->list.mitigation_device_list[0].max_mitigation_level=3;
 return 0;
}
static void teardown(struct kunit *test)
{
 struct fixture *f=test->priv;
 {int i;for(i=0;i<4;i++)if(f->sensors[i])platform_device_unregister(f->sensors[i]);}
 if(f->pdev[1])platform_device_unregister(f->pdev[1]);
 if(f->pdev[0])platform_device_unregister(f->pdev[0]);
 if(f->driver_registered)platform_driver_unregister(&qmi_device_driver);
 KUNIT_EXPECT_EQ(test,live_handles,0);
 qmi_tmd_cleanup(&f->dev);
 if(f->server2_ready)qmi_handle_release(&f->server2);
 if(f->server_ready)qmi_handle_release(&f->server);
}
static void golden_wire(struct kunit *test)
{
 struct tmd_set_mitigation_level_req_msg_v01 req={.mitigation_level=2};
 const u8 expected[]={0,0x34,0x12,0x21,0,13,0,1,6,0,5,'m','o','d','e','m',2,1,0,2};
 size_t len=40;void *wire;
 strscpy(req.mitigation_dev_id.mitigation_dev_id,"modem");
 wire=qmi_encode_message(QMI_REQUEST,0x21,&len,0x1234,tmd_set_mitigation_level_req_msg_v01_ei,&req);
 KUNIT_ASSERT_FALSE(test,IS_ERR(wire));KUNIT_EXPECT_EQ(test,len,sizeof(expected));
 KUNIT_EXPECT_MEMEQ(test,wire,expected,sizeof(expected));kfree(wire);
}
static void wire_bounds(struct kunit *test)
{
 struct tmd_set_mitigation_level_req_msg_v01 req={},decoded={};
 size_t len=40,n;void *wire;
 memset(req.mitigation_dev_id.mitigation_dev_id,'x',32);req.mitigation_level=255;
 wire=qmi_encode_message(QMI_REQUEST,0x21,&len,1,tmd_set_mitigation_level_req_msg_v01_ei,&req);
 KUNIT_ASSERT_FALSE(test,IS_ERR(wire));
 KUNIT_EXPECT_GE(test,qmi_decode_message(wire,len,tmd_set_mitigation_level_req_msg_v01_ei,&decoded),0);
 KUNIT_EXPECT_MEMEQ(test,&req,&decoded,sizeof(req));
 for(n=0;n<len;n++)KUNIT_EXPECT_LT(test,qmi_decode_message(wire,n,tmd_set_mitigation_level_req_msg_v01_ei,&decoded),0);
 kfree(wire);
}
static void malformed_list(struct kunit *test)
{
 struct fixture *f=test->priv;struct tmd_get_mitigation_device_list_resp_msg_v01 *r=&f->list;
 KUNIT_EXPECT_EQ(test,tmd_validate_list(r),0);
 r->mitigation_device_list_len=33;KUNIT_EXPECT_EQ(test,tmd_validate_list(r),-EBADMSG);r->mitigation_device_list_len=1;
 r->mitigation_device_list_valid=2;KUNIT_EXPECT_EQ(test,tmd_validate_list(r),-EBADMSG);
 r->mitigation_device_list_valid=0;KUNIT_EXPECT_EQ(test,tmd_validate_list(r),-EBADMSG);r->mitigation_device_list_valid=1;
 memset(r->mitigation_device_list[0].mitigation_dev_id.mitigation_dev_id,'x',33);
 KUNIT_EXPECT_EQ(test,tmd_validate_list(r),-EBADMSG);
 r->mitigation_device_list[0].mitigation_dev_id.mitigation_dev_id[0]=0;KUNIT_EXPECT_EQ(test,tmd_validate_list(r),-EBADMSG);
}
static void extended_duplicate(struct kunit *test)
{
 struct fixture *f=test->priv;
 f->list.mitigation_device_list_ext01_valid=1;f->list.mitigation_device_list_ext01_len=1;
 f->list.mitigation_device_list_ext01[0]=f->list.mitigation_device_list[0];
 KUNIT_EXPECT_EQ(test,tmd_validate_list(&f->list),0);KUNIT_EXPECT_EQ(test,tmd_find_cap(&f->list,"MODEM"),3);
 f->list.mitigation_device_list_ext01[0].max_mitigation_level=4;
 KUNIT_EXPECT_EQ(test,tmd_find_cap(&f->list,"modem"),-EPROTO);
 f->list.mitigation_device_list_ext01_len=65;KUNIT_EXPECT_EQ(test,tmd_validate_list(&f->list),-EBADMSG);
}
static void repeated_transactions(struct kunit *test)
{
 struct fixture *f=test->priv;int i;
 for(i=0;i<8;i++){
  KUNIT_ASSERT_EQ(test,verify_devices_and_register(&f->tmd),0);
  KUNIT_ASSERT_EQ(test,qmi_set_cur_state(f->client.cdev,i%4),0);
  mutex_lock(&f->tmd.handle.txn_lock);
  KUNIT_EXPECT_TRUE(test,idr_is_empty(&f->tmd.handle.txns));
  mutex_unlock(&f->tmd.handle.txn_lock);
 }
}
static void native_discovery(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned long state=99;
 KUNIT_ASSERT_EQ(test,verify_devices_and_register(&f->tmd),0);
 KUNIT_ASSERT_NOT_NULL(test,f->client.cdev);
 KUNIT_EXPECT_EQ(test,qmi_get_max_state(f->client.cdev,&state),0);KUNIT_EXPECT_EQ(test,state,3UL);
 KUNIT_EXPECT_EQ(test,qmi_get_cur_state(f->client.cdev,&state),0);KUNIT_EXPECT_EQ(test,state,0UL);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->calls),1);
}
static void acknowledged_set(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned long state;
 KUNIT_ASSERT_EQ(test,verify_devices_and_register(&f->tmd),0);
 KUNIT_ASSERT_EQ(test,qmi_set_cur_state(f->client.cdev,2),0);
 KUNIT_EXPECT_EQ(test,qmi_get_cur_state(f->client.cdev,&state),0);KUNIT_EXPECT_EQ(test,state,2UL);
 KUNIT_EXPECT_EQ(test,f->last_state,2);KUNIT_EXPECT_EQ(test,qmi_set_cur_state(f->client.cdev,2),0);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->calls),2);
 KUNIT_EXPECT_EQ(test,qmi_set_cur_state(f->client.cdev,4),-EINVAL);KUNIT_EXPECT_EQ(test,atomic_read(&f->calls),2);
}
static void rejected_retry(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned long state=99;
 KUNIT_ASSERT_EQ(test,verify_devices_and_register(&f->tmd),0);
 WRITE_ONCE(f->response_error,1);
 KUNIT_EXPECT_EQ(test,qmi_set_cur_state(f->client.cdev,2),-EREMOTEIO);
 KUNIT_EXPECT_EQ(test,qmi_get_cur_state(f->client.cdev,&state),-EAGAIN);KUNIT_EXPECT_EQ(test,state,99UL);
 KUNIT_EXPECT_EQ(test,qmi_set_cur_state(f->client.cdev,2),-EREMOTEIO);KUNIT_EXPECT_EQ(test,atomic_read(&f->calls),3);
 cancel_delayed_work_sync(&f->tmd.svc_arrive_work);WRITE_ONCE(f->response_error,0);
 KUNIT_EXPECT_EQ(test,qmi_set_cur_state(f->client.cdev,2),0);KUNIT_EXPECT_EQ(test,atomic_read(&f->calls),4);
}
static void timed_out_retry(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned long state;
 KUNIT_ASSERT_EQ(test,verify_devices_and_register(&f->tmd),0);WRITE_ONCE(f->drop,true);
 KUNIT_EXPECT_EQ(test,qmi_set_cur_state(f->client.cdev,1),-ETIMEDOUT);
 KUNIT_EXPECT_EQ(test,qmi_get_cur_state(f->client.cdev,&state),-EAGAIN);
 cancel_delayed_work_sync(&f->tmd.svc_arrive_work);WRITE_ONCE(f->drop,false);
 KUNIT_EXPECT_EQ(test,qmi_set_cur_state(f->client.cdev,1),0);KUNIT_EXPECT_EQ(test,atomic_read(&f->calls),3);
}
static void stale_ack(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned long state;
 KUNIT_ASSERT_EQ(test,verify_devices_and_register(&f->tmd),0);WRITE_ONCE(f->stale,true);
 KUNIT_EXPECT_EQ(test,qmi_set_cur_state(f->client.cdev,1),-ESTALE);
 KUNIT_EXPECT_EQ(test,qmi_get_cur_state(f->client.cdev,&state),-ENOTCONN);KUNIT_EXPECT_FALSE(test,f->client.applied_valid);
}
static void offline_replay(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned long state;
 KUNIT_ASSERT_EQ(test,verify_devices_and_register(&f->tmd),0);
 thermal_qmi_del_server(&f->tmd.handle,&f->service);
 KUNIT_EXPECT_EQ(test,qmi_set_cur_state(f->client.cdev,3),-ENOTCONN);KUNIT_EXPECT_EQ(test,f->client.desired_state,3U);
 KUNIT_ASSERT_EQ(test,thermal_qmi_new_server(&f->tmd.handle,&f->service),0);
 flush_delayed_work(&f->tmd.svc_arrive_work);
 KUNIT_EXPECT_EQ(test,qmi_get_cur_state(f->client.cdev,&state),0);KUNIT_EXPECT_EQ(test,state,3UL);KUNIT_EXPECT_EQ(test,f->last_state,3);
}
static void stale_peer_delete(struct kunit *test)
{
 struct fixture *f=test->priv;struct qmi_service other=f->service;
 other.port++;thermal_qmi_del_server(&f->tmd.handle,&other);KUNIT_EXPECT_TRUE(test,f->tmd.has_server);
 thermal_qmi_net_reset(&f->tmd.handle);KUNIT_EXPECT_FALSE(test,f->tmd.has_server);
}
static void changed_maximum(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned long state;
 KUNIT_ASSERT_EQ(test,verify_devices_and_register(&f->tmd),0);
 KUNIT_ASSERT_EQ(test,qmi_set_cur_state(f->client.cdev,3),0);
 f->list.mitigation_device_list[0].max_mitigation_level=1;
 KUNIT_ASSERT_EQ(test,verify_devices_and_register(&f->tmd),0);KUNIT_ASSERT_NOT_NULL(test,f->client.cdev);
 KUNIT_EXPECT_EQ(test,f->client.cdev->max_state,1UL);
 KUNIT_EXPECT_EQ(test,qmi_get_cur_state(f->client.cdev,&state),0);KUNIT_EXPECT_EQ(test,state,1UL);
}
static void no_false_registration(struct kunit *test)
{
 struct fixture *f=test->priv;
 WRITE_ONCE(f->response_error,1);KUNIT_EXPECT_EQ(test,verify_devices_and_register(&f->tmd),-EREMOTEIO);
 KUNIT_EXPECT_PTR_EQ(test,f->client.cdev,NULL);WRITE_ONCE(f->response_error,0);
 f->list.mitigation_device_list[0].mitigation_dev_id.mitigation_dev_id[0]='x';
 KUNIT_EXPECT_EQ(test,verify_devices_and_register(&f->tmd),0);KUNIT_EXPECT_PTR_EQ(test,f->client.cdev,NULL);
}
static void idempotent_cleanup(struct kunit *test)
{
 struct fixture *f=test->priv;
 KUNIT_ASSERT_EQ(test,verify_devices_and_register(&f->tmd),0);
 qmi_tmd_cleanup(&f->dev);qmi_tmd_cleanup(&f->dev);
 KUNIT_EXPECT_PTR_EQ(test,f->client.cdev,NULL);KUNIT_EXPECT_FALSE(test,f->tmd.handle_ready);
 KUNIT_EXPECT_EQ(test,thermal_qmi_new_server(&f->tmd.handle,&f->service),-ENODEV);
}

static const struct of_device_id private_match[]={{.compatible="avr,qmi-cooling-private"},{}};
static int register_private(struct fixture *f)
{
 int ret;
 qmi_device_driver.driver.name="avr-qmi-probe";qmi_device_driver.driver.of_match_table=private_match;
 ret=platform_driver_register(&qmi_device_driver);if(!ret)f->driver_registered=true;return ret;
}
static int add_private(struct fixture *f,const char *name,int slot)
{
 char path[96];struct device_node *node;struct platform_device *pdev;int ret;
 snprintf(path,sizeof(path),"/avr-qmi-fixtures/%s",name);node=of_find_node_by_path(path);if(!node)return -ENOENT;
 pdev=platform_device_alloc("avr-qmi-probe",PLATFORM_DEVID_AUTO);
 if(!pdev){of_node_put(node);return -ENOMEM;}
 device_set_node(&pdev->dev,of_fwnode_handle(node));
 ret=platform_device_add(pdev);if(ret){platform_device_put(pdev);return ret;}
 f->pdev[slot]=pdev;wait_for_device_probe();return 0;
}
static int advertise_saved(struct fixture *f)
{
 struct device_node *root=of_find_node_by_path("/avr-qmi-fixtures/good");int ret=0;unsigned int i=0;
 if(!root)return -ENOENT;
 memset(&f->list,0,sizeof(f->list));f->list.mitigation_device_list_valid=1;
 for_each_available_child_of_node_scoped(root,instance){
  for_each_available_child_of_node_scoped(instance,node){
   const char *name;ret=of_property_read_string(node,"qcom,qmi-dev-name",&name);if(ret)goto out;
   if(i>=ARRAY_SIZE(f->list.mitigation_device_list)){ret=-E2BIG;goto out;}
   strscpy(f->list.mitigation_device_list[i].mitigation_dev_id.mitigation_dev_id,name);
   f->list.mitigation_device_list[i].max_mitigation_level=3;
   if(f->map_caps){
    if(!strcmp(name,"modem_lte_dsc") || !strcmp(name,"modem_nr_dsc"))f->list.mitigation_device_list[i].max_mitigation_level=255;
    if(!strcmp(name,"modem_nr_scg_dsc"))f->list.mitigation_device_list[i].max_mitigation_level=100;
    if(!strcmp(name,"cdsp_sw"))f->list.mitigation_device_list[i].max_mitigation_level=4;
   }
   i++;
  }
 }
 f->list.mitigation_device_list_len=i;
 ret=qmi_add_server(&f->server,TMD_SERVICE_ID_V01,TMD_SERVICE_VERS_V01,0);
 if(!ret && !f->server2_ready){
  struct sockaddr_qrtr addr={.sq_family=AF_QIPCRTR};
  ret=qmi_handle_init(&f->server2,3279,NULL,handlers2);if(ret)goto out;f->server2_ready=true;addr=f->server2.sq;
  ret=kernel_bind(f->server2.sock,(struct sockaddr_unsized *)&addr,sizeof(addr));if(ret)goto out;
  ret=kernel_getsockname(f->server2.sock,(struct sockaddr *)&f->server2.sq);if(ret<0)goto out;
 }
 if(ret>=0)ret=qmi_add_server(&f->server2,TMD_SERVICE_ID_V01,TMD_SERVICE_VERS_V01,0x43);
out:
 of_node_put(root);return ret;
}
static int wait_private(struct platform_device *pdev,bool online)
{
 struct qmi_tmd_device *priv=platform_get_drvdata(pdev);unsigned long limit=jiffies+5*HZ;unsigned int i;
 if(!priv)return -ENODEV;
 do {
  unsigned int count=0,ready=0;
  for(i=0;i<priv->count;i++){
   struct qmi_tmd_instance *t=&priv->instances[i];struct qmi_cooling_device *c;
   mutex_lock(&t->state_mutex);
   list_for_each_entry(c,&t->tmd_cdev_list,qmi_node){count++;if(online ? (c->cdev && c->connection_active && c->applied_valid) : !c->connection_active)ready++;}
   mutex_unlock(&t->state_mutex);
  }
  if(count==27 && ready==count)return 0;
  usleep_range(2000,4000);
 }while(time_before(jiffies,limit));
 return -ETIMEDOUT;
}
static void native_of_27(struct kunit *test)
{
 struct fixture *f=test->priv;struct qmi_tmd_device *priv;unsigned int i,count=0;unsigned long state;
 KUNIT_ASSERT_EQ(test,register_private(f),0);KUNIT_ASSERT_EQ(test,advertise_saved(f),0);
 KUNIT_ASSERT_EQ(test,add_private(f,"good",0),0);KUNIT_ASSERT_EQ(test,wait_private(f->pdev[0],true),0);
 priv=platform_get_drvdata(f->pdev[0]);KUNIT_ASSERT_NOT_NULL(test,priv);KUNIT_EXPECT_EQ(test,priv->count,2U);
 for(i=0;i<priv->count;i++){
  struct qmi_tmd_instance *t=&priv->instances[i];struct qmi_cooling_device *c;
  list_for_each_entry(c,&t->tmd_cdev_list,qmi_node){
   KUNIT_ASSERT_NOT_NULL(test,c->cdev);KUNIT_EXPECT_STREQ(test,c->cdev->type,c->np->name);
   KUNIT_EXPECT_EQ(test,qmi_get_max_state(c->cdev,&state),0);KUNIT_EXPECT_EQ(test,state,3UL);
   KUNIT_EXPECT_EQ(test,qmi_set_cur_state(c->cdev,2),0);KUNIT_EXPECT_EQ(test,qmi_get_cur_state(c->cdev,&state),0);KUNIT_EXPECT_EQ(test,state,2UL);count++;
  }
 }
 KUNIT_EXPECT_EQ(test,count,27U);KUNIT_EXPECT_GE(test,atomic_read(&f->calls),54);
}
static void native_server_restart(struct kunit *test)
{
 struct fixture *f=test->priv;struct sockaddr_qrtr addr={.sq_family=AF_QIPCRTR};int ret;
 KUNIT_ASSERT_EQ(test,register_private(f),0);KUNIT_ASSERT_EQ(test,advertise_saved(f),0);
 KUNIT_ASSERT_EQ(test,add_private(f,"good",0),0);KUNIT_ASSERT_EQ(test,wait_private(f->pdev[0],true),0);
 qmi_handle_release(&f->server2);f->server2_ready=false;
 qmi_handle_release(&f->server);f->server_ready=false;
 KUNIT_ASSERT_EQ(test,wait_private(f->pdev[0],false),0);
 ret=qmi_handle_init(&f->server,3279,NULL,handlers);KUNIT_ASSERT_EQ(test,ret,0);f->server_ready=true;addr=f->server.sq;
 KUNIT_ASSERT_EQ(test,kernel_bind(f->server.sock,(struct sockaddr_unsized *)&addr,sizeof(addr)),0);
 KUNIT_ASSERT_GE(test,kernel_getsockname(f->server.sock,(struct sockaddr *)&f->server.sq),0);
 KUNIT_ASSERT_EQ(test,advertise_saved(f),0);KUNIT_EXPECT_EQ(test,wait_private(f->pdev[0],true),0);
}
static void native_rebind(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned int i;
 KUNIT_ASSERT_EQ(test,register_private(f),0);KUNIT_ASSERT_EQ(test,advertise_saved(f),0);
 KUNIT_ASSERT_EQ(test,add_private(f,"good",0),0);KUNIT_ASSERT_EQ(test,wait_private(f->pdev[0],true),0);
 for(i=0;i<3;i++){
  device_release_driver(&f->pdev[0]->dev);KUNIT_EXPECT_PTR_EQ(test,platform_get_drvdata(f->pdev[0]),NULL);
  KUNIT_ASSERT_EQ(test,device_attach(&f->pdev[0]->dev),1);KUNIT_ASSERT_EQ(test,wait_private(f->pdev[0],true),0);
 }
}
static void reject_dt(struct kunit *test,const char *name)
{
 struct fixture *f=test->priv;
 KUNIT_ASSERT_EQ(test,register_private(f),0);KUNIT_ASSERT_EQ(test,add_private(f,name,0),0);
 KUNIT_EXPECT_PTR_EQ(test,f->pdev[0]->dev.driver,NULL);KUNIT_EXPECT_PTR_EQ(test,platform_get_drvdata(f->pdev[0]),NULL);
}
static void native_duplicate_dt(struct kunit *test){reject_dt(test,"bad-duplicate");}
static void native_unknown_wire_dt(struct kunit *test){reject_dt(test,"bad-wire");}
static void native_wrong_cells_dt(struct kunit *test){reject_dt(test,"bad-cells");}
static void native_two_devices(struct kunit *test)
{
 struct fixture *f=test->priv;struct qmi_tmd_device *a,*b;
 KUNIT_ASSERT_EQ(test,register_private(f),0);KUNIT_ASSERT_EQ(test,advertise_saved(f),0);
 KUNIT_ASSERT_EQ(test,add_private(f,"good",0),0);KUNIT_ASSERT_EQ(test,add_private(f,"good",1),0);
 KUNIT_ASSERT_EQ(test,wait_private(f->pdev[0],true),0);KUNIT_ASSERT_EQ(test,wait_private(f->pdev[1],true),0);
 a=platform_get_drvdata(f->pdev[0]);b=platform_get_drvdata(f->pdev[1]);
 KUNIT_EXPECT_PTR_NE(test,a,b);KUNIT_EXPECT_PTR_NE(test,a->instances,b->instances);
 platform_device_unregister(f->pdev[0]);f->pdev[0]=NULL;
 KUNIT_EXPECT_EQ(test,wait_private(f->pdev[1],true),0);
}
/* Only the selected allocation/lookup failure is recorded. Every preceding
 * successful handle, lookup, callback, worker and cleanup is native. */
static void partial_probe(struct kunit *test,bool lookup,int nth)
{
 struct fixture *f=test->priv;int expected=lookup ? nth : nth-1;
 KUNIT_ASSERT_EQ(test,register_private(f),0);KUNIT_ASSERT_EQ(test,advertise_saved(f),0);
 if(lookup)fail_lookup_at=nth;else fail_init_at=nth;
 KUNIT_ASSERT_EQ(test,add_private(f,"good",0),0);
 KUNIT_EXPECT_PTR_EQ(test,f->pdev[0]->dev.driver,NULL);KUNIT_EXPECT_PTR_EQ(test,platform_get_drvdata(f->pdev[0]),NULL);
 KUNIT_EXPECT_EQ(test,live_handles,0);KUNIT_EXPECT_EQ(test,released_handles,expected);
 fail_init_at=fail_lookup_at=0;
 KUNIT_ASSERT_EQ(test,device_attach(&f->pdev[0]->dev),1);KUNIT_ASSERT_EQ(test,wait_private(f->pdev[0],true),0);
 KUNIT_EXPECT_EQ(test,live_handles,2);
 device_release_driver(&f->pdev[0]->dev);KUNIT_EXPECT_EQ(test,live_handles,0);KUNIT_EXPECT_EQ(test,released_handles,expected+2);
}
static void first_init_failure(struct kunit *test){partial_probe(test,false,1);}
static void second_init_failure(struct kunit *test){partial_probe(test,false,2);}
static void first_lookup_failure(struct kunit *test){partial_probe(test,true,1);}
static void second_lookup_failure(struct kunit *test){partial_probe(test,true,2);}

static int recorded_temperature(struct thermal_zone_device *tz,int *temp)
{*temp=READ_ONCE(*(int *)tz->devdata);return 0;}
static const struct thermal_zone_device_ops recorded_ops={.get_temp=recorded_temperature};
static int start_zones(struct fixture *f)
{
 int i,ret;
 for(i=0;i<4;i++){
  char path[48];struct device_node *np;struct platform_device *pdev;
  snprintf(path,sizeof(path),"/avr-qmi-sensor%d",i);np=of_find_node_by_path(path);if(!np)return -ENOENT;
  pdev=platform_device_alloc("avr-qmi-temp",PLATFORM_DEVID_AUTO);if(!pdev){of_node_put(np);return -ENOMEM;}
  device_set_node(&pdev->dev,of_fwnode_handle(np));ret=platform_device_add(pdev);
  if(ret){platform_device_put(pdev);return ret;}f->sensors[i]=pdev;
  f->zones[i]=devm_thermal_of_zone_register(&pdev->dev,0,&f->temps[i],&recorded_ops);
  if(IS_ERR(f->zones[i])){ret=PTR_ERR(f->zones[i]);f->zones[i]=NULL;return ret;}
 }
 return 0;
}
static struct qmi_cooling_device *find_client(struct fixture *f,const char *name)
{
 struct qmi_tmd_device *p=platform_get_drvdata(f->pdev[0]);unsigned int i;
 for(i=0;i<p->count;i++){
  struct qmi_cooling_device *c;
  list_for_each_entry(c,&p->instances[i].tmd_cdev_list,qmi_node)if(!strcmp(c->qmi_name,name))return c;
 }
 return NULL;
}
static int start_maps(struct fixture *f)
{
 int ret;f->map_caps=true;
 ret=register_private(f);if(!ret)ret=advertise_saved(f);if(!ret)ret=add_private(f,"good",0);
 if(!ret)ret=wait_private(f->pdev[0],true);if(!ret)ret=start_zones(f);return ret;
}
static void failed_read_preserves_instance(struct kunit *test)
{
 struct fixture *f=test->priv;struct qmi_cooling_device *c;struct thermal_zone_device *tz;struct thermal_instance *inst=NULL,*p;
 KUNIT_ASSERT_EQ(test,start_maps(f),0);c=find_client(f,"bcl");KUNIT_ASSERT_NOT_NULL(test,c);tz=f->zones[0];
 KUNIT_ASSERT_STREQ(test,tz->governor->name,"step_wise");
 mutex_lock(&tz->lock);
 {int i;for(i=0;i<tz->num_trips;i++)list_for_each_entry(p,&tz->trips[i].thermal_instances,trip_node)if(p->cdev==c->cdev){inst=p;break;}}
 if(!inst){mutex_unlock(&tz->lock);KUNIT_FAIL(test,"saved BCL map missing");return;}
 mutex_lock(&c->tmd->state_mutex);c->applied_valid=false;mutex_unlock(&c->tmd->state_mutex);
 inst->initialized=false;inst->target=THERMAL_NO_TARGET;
 mutex_lock(&c->cdev->lock);c->cdev->updated=true;mutex_unlock(&c->cdev->lock);
 tz->temperature=2;tz->last_temperature=1;tz->governor->manage(tz);
 KUNIT_EXPECT_FALSE(test,inst->initialized);KUNIT_EXPECT_EQ(test,inst->target,THERMAL_NO_TARGET);
 mutex_unlock(&tz->lock);
}

static void drive_zone(struct fixture *f,int id,int temp)
{
 WRITE_ONCE(f->temps[id],temp);thermal_zone_device_update(f->zones[id],THERMAL_EVENT_UNSPECIFIED);
 cancel_delayed_work_sync(&f->zones[id]->poll_queue);
}
static void expect_state(struct kunit *test,const char *wire,unsigned long expected)
{
 struct fixture *f=test->priv;struct qmi_cooling_device *c=find_client(f,wire);unsigned long state=~0UL;
 KUNIT_ASSERT_NOT_NULL(test,c);KUNIT_ASSERT_EQ(test,qmi_get_cur_state(c->cdev,&state),0);KUNIT_EXPECT_EQ(test,state,expected);
}
static void native_saved_maps(struct kunit *test)
{
 struct fixture *f=test->priv;int i,j,total=0;
 KUNIT_ASSERT_EQ(test,start_maps(f),0);
 for(i=0;i<4;i++){
  struct thermal_zone_device *tz=f->zones[i];int count=0;
  mutex_lock(&tz->lock);
  for(j=0;j<tz->num_trips;j++){
   struct thermal_instance *inst;
   list_for_each_entry(inst,&tz->trips[j].thermal_instances,trip_node){
    struct qmi_cooling_device *c=inst->cdev->devdata;count++;
    if(i<2){KUNIT_EXPECT_STREQ(test,c->qmi_name,"bcl");KUNIT_EXPECT_EQ(test,inst->lower,0UL);KUNIT_EXPECT_EQ(test,inst->upper,(unsigned long)i+1);}
    if(i==2){KUNIT_EXPECT_STREQ(test,c->qmi_name,"cdsp_sw");KUNIT_EXPECT_EQ(test,inst->upper,4UL);}
   }
  }
  mutex_unlock(&tz->lock);KUNIT_EXPECT_EQ(test,count,i==3 ? 5 : 1);total+=count;
 }
 KUNIT_EXPECT_EQ(test,total,8);
}
static void native_modem_governor(struct kunit *test)
{
 struct fixture *f=test->priv;
 KUNIT_ASSERT_EQ(test,start_maps(f),0);drive_zone(f,3,103000);
 expect_state(test,"modem_lte_dsc",8);expect_state(test,"modem_nr_dsc",10);expect_state(test,"modem_nr_scg_dsc",100);
 drive_zone(f,3,106000);expect_state(test,"modem_lte_dsc",255);expect_state(test,"modem_nr_dsc",255);
 drive_zone(f,3,90000);expect_state(test,"modem_lte_dsc",0);expect_state(test,"modem_nr_dsc",0);expect_state(test,"modem_nr_scg_dsc",0);
}
static void native_shared_bcl_governor(struct kunit *test)
{
 struct fixture *f=test->priv;
 KUNIT_ASSERT_EQ(test,start_maps(f),0);drive_zone(f,0,2);expect_state(test,"bcl",1);
 drive_zone(f,1,2);expect_state(test,"bcl",2);
 drive_zone(f,0,0);expect_state(test,"bcl",2);
 drive_zone(f,1,0);expect_state(test,"bcl",0);
}
static void native_cdsp_governor(struct kunit *test)
{
 struct fixture *f=test->priv;int i;
 KUNIT_ASSERT_EQ(test,start_maps(f),0);
 for(i=0;i<4;i++)drive_zone(f,2,96+i);
 expect_state(test,"cdsp_sw",4);drive_zone(f,2,80);expect_state(test,"cdsp_sw",0);
}
static void native_governor_rpc_recovery(struct kunit *test)
{
 struct fixture *f=test->priv;struct qmi_cooling_device *c;unsigned long state;
 KUNIT_ASSERT_EQ(test,start_maps(f),0);c=find_client(f,"bcl");KUNIT_ASSERT_NOT_NULL(test,c);
 WRITE_ONCE(f->response_error,1);drive_zone(f,0,2);
 KUNIT_EXPECT_EQ(test,qmi_get_cur_state(c->cdev,&state),-EAGAIN);KUNIT_EXPECT_EQ(test,c->desired_state,1U);
 cancel_delayed_work_sync(&c->tmd->svc_arrive_work);
 /* A subsequent governor pass must retain its target while state is unknown. */
 drive_zone(f,0,3);KUNIT_EXPECT_EQ(test,c->desired_state,1U);
 cancel_delayed_work_sync(&c->tmd->svc_arrive_work);WRITE_ONCE(f->response_error,0);
 mod_delayed_work(system_dfl_wq,&c->tmd->svc_arrive_work,0);flush_delayed_work(&c->tmd->svc_arrive_work);
 expect_state(test,"bcl",1);drive_zone(f,0,0);expect_state(test,"bcl",0);
}
static struct kunit_case cases[]={
 KUNIT_CASE(golden_wire),KUNIT_CASE(wire_bounds),KUNIT_CASE(malformed_list),KUNIT_CASE(extended_duplicate),
 KUNIT_CASE(repeated_transactions),KUNIT_CASE(native_discovery),KUNIT_CASE(acknowledged_set),KUNIT_CASE(rejected_retry),KUNIT_CASE(timed_out_retry),
 KUNIT_CASE(stale_ack),KUNIT_CASE(offline_replay),KUNIT_CASE(stale_peer_delete),KUNIT_CASE(changed_maximum),
 KUNIT_CASE(no_false_registration),KUNIT_CASE(idempotent_cleanup),
 KUNIT_CASE(native_of_27),KUNIT_CASE(native_server_restart),KUNIT_CASE(native_rebind),
 KUNIT_CASE(native_duplicate_dt),KUNIT_CASE(native_unknown_wire_dt),KUNIT_CASE(native_wrong_cells_dt),KUNIT_CASE(native_two_devices),
 KUNIT_CASE(first_init_failure),KUNIT_CASE(second_init_failure),KUNIT_CASE(first_lookup_failure),KUNIT_CASE(second_lookup_failure),KUNIT_CASE(failed_read_preserves_instance),
 KUNIT_CASE(native_saved_maps),KUNIT_CASE(native_modem_governor),KUNIT_CASE(native_shared_bcl_governor),
 KUNIT_CASE(native_cdsp_governor),KUNIT_CASE(native_governor_rpc_recovery),{}
};
static struct kunit_suite suite={.name="avr-qmi-cooling",.init=setup,.exit=teardown,.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");

// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * Native 7.3 ownership, confirmed-state and server-generation port.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/soc/qcom/qmi.h>
#include "thermal_mitigation_device_service_v01.h"
#define QMI_CDEV_DRIVER "qmi-cooling-device"
#define QMI_TMD_RESP_TOUT msecs_to_jiffies(100)
#define QMI_TMD_RETRY msecs_to_jiffies(1000)
struct qmi_tmd_instance;
struct qmi_cooling_device {
 struct device_node *np;
 const char *cdev_name;
 char qmi_name[QMI_TMD_MITIGATION_DEV_ID_LENGTH_MAX_V01 + 1];
 struct list_head qmi_node;
 struct thermal_cooling_device *cdev;
 struct qmi_tmd_instance *tmd;
 unsigned int desired_state,applied_state,max_level;
 bool connection_active,applied_valid,cap_valid;
};
struct qmi_tmd_instance {
 struct device *dev;
 struct qmi_handle handle;
 /* Never hold state_mutex while entering native thermal registration/remove.
  * QMI callbacks only take state_mutex; they must not wait for I/O completion. */
 struct mutex state_mutex,io_mutex;
 u32 inst_id;
 u64 generation;
 struct sockaddr_qrtr peer;
 struct list_head tmd_cdev_list;
 struct delayed_work svc_arrive_work;
 unsigned int count;
 bool stopping,has_server,handle_ready;
};
struct qmi_tmd_device {
 struct qmi_tmd_instance *instances;
 unsigned int count;
};
static const char device_clients[][QMI_TMD_MITIGATION_DEV_ID_LENGTH_MAX_V01 + 1] = {
	{"pa"},
	{"pa_fr1"},
	{"cx_vdd_limit"},
	{"modem"},
	{"modem_current"},
	{"modem_skin"},
	{"modem_bw"},
	{"modem_bw_backoff"},
	{"vbatt_low"},
	{"charge_state"},
	{"mmw0"},
	{"mmw1"},
	{"mmw2"},
	{"mmw3"},
	{"mmw_skin0"},
	{"mmw_skin1"},
	{"mmw_skin2"},
	{"mmw_skin3"},
	{"wlan"},
	{"wlan_bw"},
	{"mmw_skin0_dsc"},
	{"mmw_skin1_dsc"},
	{"mmw_skin2_dsc"},
	{"mmw_skin3_dsc"},
	{"modem_skin_lte_dsc"},
	{"modem_skin_nr_dsc"},
	{"pa_dsc"},
	{"pa_fr1_dsc"},
	{"cdsp_sw"},
	{"cdsp_sw_hvx"},
	{"cdsp_sw_hmx"},
	{"cdsp_hw"},
	{"cpuv_restriction_cold"},
	{"cpr_cold"},
	{"modem_lte_dsc"},
	{"modem_nr_dsc"},
	{"modem_nr_scg_dsc"},
	{"sdr0_lte_dsc"},
	{"sdr1_lte_dsc"},
	{"sdr0_nr_dsc"},
	{"sdr1_nr_dsc"},
	{"sdr0_nr_scg_dsc"},
	{"sdr1_nr_scg_dsc"},
	{"pa_lte_sdr0_dsc"},
	{"pa_lte_sdr1_dsc"},
	{"pa_nr_sdr0_dsc"},
	{"pa_nr_sdr1_dsc"},
	{"pa_nr_sdr0_scg_dsc"},
	{"pa_nr_sdr1_scg_dsc"},
	{"mmw0_dsc"},
	{"mmw1_dsc"},
	{"mmw2_dsc"},
	{"mmw3_dsc"},
	{"mmw_ific_dsc"},
	{"modem_lte_sub1_dsc"},
	{"modem_nr_sub1_dsc"},
	{"modem_nr_scg_sub1_dsc"},
	{"sdr0_lte_sub1_dsc"},
	{"sdr1_lte_sub1_dsc"},
	{"sdr0_nr_sub1_dsc"},
	{"sdr1_nr_sub1_dsc"},
	{"pa_lte_sdr0_sub1_dsc"},
	{"pa_lte_sdr1_sub1_dsc"},
	{"pa_nr_sdr0_sub1_dsc"},
	{"pa_nr_sdr1_sub1_dsc"},
	{"pa_nr_sdr0_scg_sub1_dsc"},
	{"pa_nr_sdr1_scg_sub1_dsc"},
	{"mmw0_sub1_dsc"},
	{"mmw1_sub1_dsc"},
	{"mmw2_sub1_dsc"},
	{"mmw3_sub1_dsc"},
	{"mmw_ific_sub1_dsc"},
	{"bcl"},
	{"modem_nr_1rx_dsc"},
	{"modem_nr_2rx_dsc"},
	{"modem_bcl_mtpl_dsc"},
};
static bool qmi_name_supported(const char *name)
{
 unsigned int i;
 for(i=0;i<ARRAY_SIZE(device_clients);i++)
  if(!strcmp(name,device_clients[i]))return true;
 return false;
}
static void tmd_offline_locked(struct qmi_tmd_instance *tmd)
{
 struct qmi_cooling_device *c;
 tmd->has_server=false;tmd->generation++;
 list_for_each_entry(c,&tmd->tmd_cdev_list,qmi_node){c->connection_active=false;c->applied_valid=false;}
}
static int qmi_get_max_state(struct thermal_cooling_device *cdev,unsigned long *state)
{
 struct qmi_cooling_device *c=cdev->devdata;int ret=0;
 mutex_lock(&c->tmd->state_mutex);
 if(c->tmd->stopping)ret=-ENODEV;
 else if(!c->cap_valid)ret=-EAGAIN;
 else *state=c->max_level;
 mutex_unlock(&c->tmd->state_mutex);return ret;
}
static int qmi_get_cur_state(struct thermal_cooling_device *cdev,unsigned long *state)
{
 struct qmi_cooling_device *c=cdev->devdata;int ret=0;
 mutex_lock(&c->tmd->state_mutex);
 if(c->tmd->stopping)ret=-ENODEV;
 else if(!c->connection_active)ret=-ENOTCONN;
 else if(!c->applied_valid)ret=-EAGAIN;
 else *state=c->applied_state;
 mutex_unlock(&c->tmd->state_mutex);return ret;
}
static int tmd_exchange(struct qmi_tmd_instance *tmd,struct sockaddr_qrtr *peer,
 unsigned int id,size_t maxlen,const struct qmi_elem_info *request_ei,
 const void *request,const struct qmi_elem_info *response_ei,void *response)
{
 struct qmi_txn txn;int ret;
 ret=qmi_txn_init(&tmd->handle,&txn,response_ei,response);if(ret<0)return ret;
 ret=qmi_send_request(&tmd->handle,peer,&txn,id,maxlen,request_ei,request);
 if(ret<0){qmi_txn_cancel(&txn);return ret;}
 ret=qmi_txn_wait(&txn,QMI_TMD_RESP_TOUT);
 /* Native decoder returns a nonnegative decoded-element count on success. */
 return ret<0 ? ret : 0;
}
/* io_mutex is held. Publish only an acknowledgement from the same peer epoch. */
static int tmd_apply(struct qmi_cooling_device *c)
{
 struct qmi_tmd_instance *tmd=c->tmd;
 struct tmd_set_mitigation_level_req_msg_v01 req={};
 struct tmd_set_mitigation_level_resp_msg_v01 resp={};
 struct sockaddr_qrtr peer;u64 generation;unsigned int state;int ret;
 mutex_lock(&tmd->state_mutex);
 if(tmd->stopping){ret=-ENODEV;goto early;}
 if(!tmd->has_server || !c->connection_active){ret=-ENOTCONN;goto early;}
 state=c->desired_state;
 if(state>c->max_level || state>U8_MAX){ret=-ERANGE;goto early;}
 if(c->applied_valid && c->applied_state==state){ret=0;goto early;}
 generation=tmd->generation;peer=tmd->peer;
 mutex_unlock(&tmd->state_mutex);
 strscpy(req.mitigation_dev_id.mitigation_dev_id,c->qmi_name,sizeof(req.mitigation_dev_id.mitigation_dev_id));req.mitigation_level=state;
 ret=tmd_exchange(tmd,&peer,QMI_TMD_SET_MITIGATION_LEVEL_REQ_V01,
  TMD_SET_MITIGATION_LEVEL_REQ_MSG_V01_MAX_MSG_LEN,tmd_set_mitigation_level_req_msg_v01_ei,&req,
  tmd_set_mitigation_level_resp_msg_v01_ei,&resp);
 if(!ret && resp.resp.result!=QMI_RESULT_SUCCESS_V01)ret=-EREMOTEIO;
 mutex_lock(&tmd->state_mutex);
 if(tmd->stopping)ret=-ENODEV;
 else if(!tmd->has_server || !c->connection_active || generation!=tmd->generation)ret=-ESTALE;
 c->applied_valid=!ret;
 if(!ret)c->applied_state=state;
early:
 mutex_unlock(&tmd->state_mutex);return ret;
}
static int qmi_set_cur_state(struct thermal_cooling_device *cdev,unsigned long state)
{
 struct qmi_cooling_device *c=cdev->devdata;struct qmi_tmd_instance *tmd=c->tmd;int ret;
 mutex_lock(&tmd->io_mutex);mutex_lock(&tmd->state_mutex);
 if(tmd->stopping)ret=-ENODEV;
 else if(!c->cap_valid || state>c->max_level || state>U8_MAX)ret=-EINVAL;
 else {c->desired_state=state;ret=0;}
 mutex_unlock(&tmd->state_mutex);
 if(!ret)ret=tmd_apply(c);
 mutex_unlock(&tmd->io_mutex);
 if(ret && ret!=-EINVAL){
  mutex_lock(&tmd->state_mutex);
  if(!tmd->stopping && tmd->has_server)mod_delayed_work(system_dfl_wq,&tmd->svc_arrive_work,QMI_TMD_RETRY);
  mutex_unlock(&tmd->state_mutex);
 }
 return ret;
}
static const struct thermal_cooling_device_ops qmi_device_ops={.get_max_state=qmi_get_max_state,.get_cur_state=qmi_get_cur_state,.set_cur_state=qmi_set_cur_state};
static int tmd_validate_list(const struct tmd_get_mitigation_device_list_resp_msg_v01 *r)
{
 unsigned int i;
 if(r->resp.result!=QMI_RESULT_SUCCESS_V01)return -EREMOTEIO;
 if(r->mitigation_device_list_valid>1 || r->mitigation_device_list_ext01_valid>1 ||
  r->mitigation_device_list_len>QMI_TMD_MITIGATION_DEV_LIST_MAX_V01 ||
  r->mitigation_device_list_ext01_len>QMI_TMD_MITIGATION_DEV_LIST_EXT01_MAX_V01 ||
  (!r->mitigation_device_list_valid && r->mitigation_device_list_len) ||
  (!r->mitigation_device_list_ext01_valid && r->mitigation_device_list_ext01_len))return -EBADMSG;
 for(i=0;i<r->mitigation_device_list_len+r->mitigation_device_list_ext01_len;i++){
  const struct tmd_mitigation_dev_list_type_v01 *p=i<r->mitigation_device_list_len ? &r->mitigation_device_list[i] : &r->mitigation_device_list_ext01[i-r->mitigation_device_list_len];
  if(!p->mitigation_dev_id.mitigation_dev_id[0] || !memchr(p->mitigation_dev_id.mitigation_dev_id,0,sizeof(p->mitigation_dev_id.mitigation_dev_id)))return -EBADMSG;
 }
 return 0;
}
static int tmd_find_cap(const struct tmd_get_mitigation_device_list_resp_msg_v01 *r,const char *name)
{
 unsigned int i;int cap=-ENOENT;
 for(i=0;i<r->mitigation_device_list_len+r->mitigation_device_list_ext01_len;i++){
  const struct tmd_mitigation_dev_list_type_v01 *p=i<r->mitigation_device_list_len ? &r->mitigation_device_list[i] : &r->mitigation_device_list_ext01[i-r->mitigation_device_list_len];
  if(strcasecmp(name,p->mitigation_dev_id.mitigation_dev_id))continue;
  if(cap>=0 && cap!=p->max_mitigation_level)return -EPROTO;
  cap=p->max_mitigation_level;
 }
 return cap;
}
static int verify_devices_and_register(struct qmi_tmd_instance *tmd)
{
 struct tmd_get_mitigation_device_list_req_msg_v01 req={};
 struct tmd_get_mitigation_device_list_resp_msg_v01 *resp;
 struct qmi_cooling_device *c;struct sockaddr_qrtr peer;u64 generation;
 int *caps,ret=0;unsigned int i=0;
 resp=kzalloc(sizeof(*resp),GFP_KERNEL);caps=kcalloc(tmd->count,sizeof(*caps),GFP_KERNEL);
 if(!resp || !caps){ret=-ENOMEM;goto out;}
 mutex_lock(&tmd->io_mutex);mutex_lock(&tmd->state_mutex);
 if(tmd->stopping || !tmd->has_server){ret=-ENOTCONN;mutex_unlock(&tmd->state_mutex);goto unlock_io;}
 peer=tmd->peer;generation=tmd->generation;mutex_unlock(&tmd->state_mutex);
 ret=tmd_exchange(tmd,&peer,QMI_TMD_GET_MITIGATION_DEVICE_LIST_REQ_V01,
  TMD_GET_MITIGATION_DEVICE_LIST_REQ_MSG_V01_MAX_MSG_LEN,tmd_get_mitigation_device_list_req_msg_v01_ei,&req,
  tmd_get_mitigation_device_list_resp_msg_v01_ei,resp);
 if(!ret)ret=tmd_validate_list(resp);
 if(!ret)list_for_each_entry(c,&tmd->tmd_cdev_list,qmi_node){caps[i]=tmd_find_cap(resp,c->qmi_name);if(caps[i]==-EPROTO){ret=-EPROTO;break;}i++;}
unlock_io:
 mutex_unlock(&tmd->io_mutex);if(ret)goto out;
 i=0;
 list_for_each_entry(c,&tmd->tmd_cdev_list,qmi_node){
  struct thermal_cooling_device *update=NULL,*new;bool create;int cap=caps[i++],error;
  mutex_lock(&tmd->state_mutex);
  if(tmd->stopping || !tmd->has_server || tmd->generation!=generation){mutex_unlock(&tmd->state_mutex);ret=-ESTALE;break;}
  if(cap<0){c->connection_active=false;c->applied_valid=false;mutex_unlock(&tmd->state_mutex);continue;}
  if(c->cdev && c->max_level!=cap){update=c->cdev;c->applied_valid=false;}
  mutex_unlock(&tmd->state_mutex);
  /* Preserve native DT bindings across capability changes. */
  mutex_lock(&tmd->io_mutex);mutex_lock(&tmd->state_mutex);
  if(tmd->stopping || !tmd->has_server || tmd->generation!=generation){mutex_unlock(&tmd->state_mutex);mutex_unlock(&tmd->io_mutex);ret=-ESTALE;break;}
  c->max_level=cap;c->cap_valid=true;c->connection_active=true;
  if(c->desired_state>cap)c->desired_state=cap;
  mutex_unlock(&tmd->state_mutex);
  error=tmd_apply(c);mutex_unlock(&tmd->io_mutex);
  /* Native update takes the cdev lock and invokes driver callbacks. */
  if(update)thermal_cooling_device_update(update);
  if(error){ret=error;continue;}
  mutex_lock(&tmd->state_mutex);create=!tmd->stopping && !c->cdev;mutex_unlock(&tmd->state_mutex);
  if(!create)continue;
  new=thermal_of_cooling_device_register(c->np,0,c->cdev_name,c,&qmi_device_ops);
  if(IS_ERR(new)){ret=PTR_ERR(new);continue;}
  mutex_lock(&tmd->state_mutex);
  if(tmd->stopping){mutex_unlock(&tmd->state_mutex);thermal_cooling_device_unregister(new);ret=-ENODEV;break;}
  c->cdev=new;mutex_unlock(&tmd->state_mutex);
 }
out:
 kfree(caps);kfree(resp);return ret;
}
static void qmi_tmd_svc_arrive(struct work_struct *work)
{
 struct qmi_tmd_instance *tmd=container_of(to_delayed_work(work),struct qmi_tmd_instance,svc_arrive_work);int ret;
 ret=verify_devices_and_register(tmd);
 if(ret){mutex_lock(&tmd->state_mutex);if(!tmd->stopping && tmd->has_server)mod_delayed_work(system_dfl_wq,&tmd->svc_arrive_work,QMI_TMD_RETRY);mutex_unlock(&tmd->state_mutex);}
}
static int thermal_qmi_new_server(struct qmi_handle *qmi,struct qmi_service *service)
{
 struct qmi_tmd_instance *tmd=container_of(qmi,struct qmi_tmd_instance,handle);int ret=0;
 mutex_lock(&tmd->state_mutex);
 if(tmd->stopping)ret=-ENODEV;
 else {
  if(!tmd->has_server || tmd->peer.sq_node!=service->node || tmd->peer.sq_port!=service->port)tmd_offline_locked(tmd);
  tmd->peer=(struct sockaddr_qrtr){.sq_family=AF_QIPCRTR,.sq_node=service->node,.sq_port=service->port};tmd->has_server=true;
  mod_delayed_work(system_dfl_wq,&tmd->svc_arrive_work,0);
 }
 mutex_unlock(&tmd->state_mutex);return ret;
}
static void thermal_qmi_del_server(struct qmi_handle *qmi,struct qmi_service *service)
{
 struct qmi_tmd_instance *tmd=container_of(qmi,struct qmi_tmd_instance,handle);
 mutex_lock(&tmd->state_mutex);
 if(tmd->has_server && tmd->peer.sq_node==service->node && tmd->peer.sq_port==service->port)tmd_offline_locked(tmd);
 mutex_unlock(&tmd->state_mutex);
}
static void thermal_qmi_net_reset(struct qmi_handle *qmi)
{
 struct qmi_tmd_instance *tmd=container_of(qmi,struct qmi_tmd_instance,handle);
 mutex_lock(&tmd->state_mutex);tmd_offline_locked(tmd);mutex_unlock(&tmd->state_mutex);
}
static const struct qmi_ops thermal_qmi_event_ops={.new_server=thermal_qmi_new_server,.del_server=thermal_qmi_del_server,.net_reset=thermal_qmi_net_reset};
static void qmi_tmd_cleanup(void *data)
{
 struct qmi_tmd_device *dev=data;unsigned int i;
 for(i=0;i<dev->count;i++){
  struct qmi_tmd_instance *t=&dev->instances[i];
  mutex_lock(&t->state_mutex);t->stopping=true;tmd_offline_locked(t);mutex_unlock(&t->state_mutex);
 }
 for(i=0;i<dev->count;i++){
  struct qmi_tmd_instance *t=&dev->instances[i];struct qmi_cooling_device *c;
  cancel_delayed_work_sync(&t->svc_arrive_work);
  mutex_lock(&t->io_mutex);if(t->handle_ready){qmi_handle_release(&t->handle);t->handle_ready=false;}mutex_unlock(&t->io_mutex);
  list_for_each_entry(c,&t->tmd_cdev_list,qmi_node){struct thermal_cooling_device *old;
   mutex_lock(&t->state_mutex);old=c->cdev;c->cdev=NULL;mutex_unlock(&t->state_mutex);
   if(old)thermal_cooling_device_unregister(old);
  }
 }
}
static void put_node(void *data){of_node_put(data);}
static int of_get_qmi_tmd_platform_data(struct device *dev,struct qmi_tmd_device *priv)
{
 struct qmi_tmd_instance *tmd;unsigned int count=of_get_available_child_count(dev->of_node),idx=0,i;int ret;
 if(!count)return -EINVAL;
 priv->instances=devm_kcalloc(dev,count,sizeof(*tmd),GFP_KERNEL);if(!priv->instances)return -ENOMEM;
 priv->count=count;
 for(i=0;i<count;i++){tmd=&priv->instances[i];tmd->dev=dev;mutex_init(&tmd->state_mutex);mutex_init(&tmd->io_mutex);INIT_LIST_HEAD(&tmd->tmd_cdev_list);INIT_DELAYED_WORK(&tmd->svc_arrive_work,qmi_tmd_svc_arrive);}
 for_each_available_child_of_node_scoped(dev->of_node,subsys){
  tmd=&priv->instances[idx++];ret=of_property_read_u32(subsys,"qcom,instance-id",&tmd->inst_id);if(ret)return ret;
  if(tmd->inst_id>0xffffff)return -ERANGE;
  for(i=0;i+1<idx;i++)if(priv->instances[i].inst_id==tmd->inst_id)return -EEXIST;
  for_each_available_child_of_node_scoped(subsys,node){
   struct qmi_cooling_device *c,*other;const char *name;u32 cells;
   ret=of_property_read_string(node,"qcom,qmi-dev-name",&name);if(ret)return ret;
   if(!qmi_name_supported(name))return -EINVAL;
   ret=of_property_read_u32(node,"#cooling-cells",&cells);if(ret || cells!=2)return -EINVAL;
   list_for_each_entry(other,&tmd->tmd_cdev_list,qmi_node)if(!strcasecmp(other->qmi_name,name))return -EEXIST;
   c=devm_kzalloc(dev,sizeof(*c),GFP_KERNEL);if(!c)return -ENOMEM;
   if(strscpy(c->qmi_name,name,sizeof(c->qmi_name))<0)return -E2BIG;
   c->cdev_name=devm_kstrdup(dev,node->name,GFP_KERNEL);if(!c->cdev_name)return -ENOMEM;
   c->np=of_node_get(node);ret=devm_add_action_or_reset(dev,put_node,c->np);if(ret)return ret;
   c->tmd=tmd;list_add_tail(&c->qmi_node,&tmd->tmd_cdev_list);tmd->count++;
  }
  if(!tmd->count)return -EINVAL;
 }
 return 0;
}
static int qmi_device_probe(struct platform_device *pdev)
{
 struct qmi_tmd_device *priv;unsigned int i;int ret;
 priv=devm_kzalloc(&pdev->dev,sizeof(*priv),GFP_KERNEL);if(!priv)return -ENOMEM;
 ret=of_get_qmi_tmd_platform_data(&pdev->dev,priv);if(ret)return ret;
 ret=devm_add_action_or_reset(&pdev->dev,qmi_tmd_cleanup,priv);if(ret)return ret;
 for(i=0;i<priv->count;i++){
  struct qmi_tmd_instance *tmd=&priv->instances[i];
  ret=qmi_handle_init(&tmd->handle,TMD_GET_MITIGATION_DEVICE_LIST_RESP_MSG_V01_MAX_MSG_LEN,&thermal_qmi_event_ops,NULL);if(ret)return ret;
  tmd->handle_ready=true;ret=qmi_add_lookup(&tmd->handle,TMD_SERVICE_ID_V01,TMD_SERVICE_VERS_V01,tmd->inst_id);if(ret)return ret;
 }
 platform_set_drvdata(pdev,priv);return 0;
}
static void qmi_device_remove(struct platform_device *pdev)
{
 struct qmi_tmd_device *priv=platform_get_drvdata(pdev);if(priv)qmi_tmd_cleanup(priv);platform_set_drvdata(pdev,NULL);
}
static const struct of_device_id qmi_device_match[]={{.compatible="qcom,qmi-cooling-devices"},{}};
MODULE_DEVICE_TABLE(of,qmi_device_match);
static struct platform_driver qmi_device_driver={.probe=qmi_device_probe,.remove=qmi_device_remove,.driver={.name=QMI_CDEV_DRIVER,.of_match_table=qmi_device_match}};
module_platform_driver(qmi_device_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Qualcomm QMI thermal mitigation confirmed-state draft");

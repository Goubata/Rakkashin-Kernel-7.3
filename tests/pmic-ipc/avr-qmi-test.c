// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/soc/qcom/qmi.h>
#include <linux/soc/qcom/pdr.h>
struct request { u32 value; };
static const struct qmi_elem_info request_ei[] = {
	{ .data_type = QMI_UNSIGNED_4_BYTE, .elem_len = 1, .elem_size = sizeof(u32),
	  .array_type = NO_ARRAY, .tlv_type = 1, .offset = offsetof(struct request, value) },
	{ .data_type = QMI_EOTI },
};
static void golden_wire(struct kunit *test)
{
	struct request request = { .value = 0x78563412 };
	u8 expected[] = {0, 0x34, 0x12, 0x56, 0, 7, 0, 1, 4, 0, 0x12, 0x34, 0x56, 0x78};
	size_t len = 7;
	void *wire = qmi_encode_message(QMI_REQUEST, 0x56, &len, 0x1234, request_ei, &request);
	KUNIT_ASSERT_FALSE(test, IS_ERR(wire));
	KUNIT_EXPECT_EQ(test, len, sizeof(expected));
	KUNIT_EXPECT_MEMEQ(test, wire, expected, sizeof(expected));
	kfree(wire);
}
static void round_trip(struct kunit *test)
{
	struct request request, result;
	u32 values[] = {0, 1, 0x80000000, U32_MAX};
	unsigned int i;
	for (i = 0; i < ARRAY_SIZE(values); i++) {
		size_t len = 7;
		void *wire;
		request.value = values[i]; result.value = 0;
		wire = qmi_encode_message(QMI_RESPONSE, 0x56, &len, 1, request_ei, &request);
		KUNIT_ASSERT_FALSE(test, IS_ERR(wire));
		KUNIT_EXPECT_GE(test, qmi_decode_message(wire, len, request_ei, &result), 0);
		KUNIT_EXPECT_EQ(test, result.value, values[i]);
		kfree(wire);
	}
}
static void malformed_tlv(struct kunit *test)
{
	/* Complete QMI header, truncated mandatory 4-byte TLV value. */
	u8 wire[] = {0, 1, 0, 0x56, 0, 4, 0, 1, 4, 0, 0x12};
	struct request result = {};
	KUNIT_EXPECT_LT(test, qmi_decode_message(wire, sizeof(wire), request_ei, &result), 0);
	KUNIT_EXPECT_EQ(test, qmi_decode_message(wire, sizeof(wire), NULL, &result), -EINVAL);
}
static void qmi_handle_lifecycle(struct kunit *test)
{
	struct qmi_handle handle;
	unsigned int i;
	for (i = 0; i < 2; i++) {
		KUNIT_ASSERT_EQ(test, qmi_handle_init(&handle, 64, NULL, NULL), 0);
		KUNIT_EXPECT_NOT_NULL(test, handle.sock);
		qmi_handle_release(&handle);
	}
	KUNIT_EXPECT_EQ(test, PTR_ERR(pdr_handle_alloc(NULL, NULL)), -EINVAL);
	pdr_handle_release(NULL);
}
static void truncated_frames(struct kunit *test)
{
	struct request input = { .value = 0x12345678 }, output;
	size_t len = 7, short_len;
	void *wire = qmi_encode_message(QMI_REQUEST, 1, &len, 1, request_ei, &input);
	KUNIT_ASSERT_FALSE(test, IS_ERR(wire));
	for (short_len = 0; short_len < len; short_len++)
		KUNIT_EXPECT_LT(test, qmi_decode_message(wire, short_len, request_ei, &output), 0);
	kfree(wire);
}
static void short_value_and_tlv_header(struct kunit *test)
{
	u8 short_value[] = {0, 1, 0, 1, 0, 4, 0, 1, 1, 0, 0x12};
	u8 short_tlv[] = {0, 1, 0, 1, 0, 2, 0, 1, 4};
	struct request output = { .value = 0xabcdef12 };
	KUNIT_EXPECT_LT(test, qmi_decode_message(short_value, sizeof(short_value), request_ei, &output), 0);
	KUNIT_EXPECT_LT(test, qmi_decode_message(short_tlv, sizeof(short_tlv), request_ei, &output), 0);
	KUNIT_EXPECT_EQ(test, output.value, 0xabcdef12U);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(golden_wire), KUNIT_CASE(round_trip), KUNIT_CASE(malformed_tlv),
	KUNIT_CASE(qmi_handle_lifecycle), KUNIT_CASE(truncated_frames),
	KUNIT_CASE(short_value_and_tlv_header), {}
};
static struct kunit_suite suite = { .name = "avr-qmi", .test_cases = cases };
kunit_test_suite(suite);
MODULE_DESCRIPTION("AVR native QMI wire and handle tests");
MODULE_LICENSE("GPL");

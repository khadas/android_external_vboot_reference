/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for misc library
 */

#include <stdio.h>

#include "2sysincludes.h"
#include "2api.h"
#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2rsa.h"
#include "2secdata.h"

#include "test_common.h"

/* Common context for tests */
static uint8_t workbuf[VB2_WORKBUF_RECOMMENDED_SIZE]
	__attribute__ ((aligned (16)));
static struct vb2_context cc;
static struct vb2_shared_data *sd;

const char mock_body[320] = "Mock body";
const int mock_body_size = sizeof(mock_body);
const int mock_algorithm = VB2_ALG_RSA2048_SHA256;
const int mock_sig_size = 64;

/* Mocked function data */

static int retval_vb2_fw_parse_gbb;
static int retval_vb2_check_dev_switch;
static int retval_vb2_check_tpm_clear;
static int retval_vb2_select_fw_slot;
static int retval_vb2_verify_fw_keyblock;
static int retval_vb2_verify_fw_preamble2;
static int retval_vb2_digest_finalize;
static int retval_vb2_verify_digest;

/* Type of test to reset for */
enum reset_type {
	FOR_MISC,
	FOR_EXTEND_HASH,
	FOR_CHECK_HASH,
};

static void reset_common_data(enum reset_type t)
{
	struct vb2_fw_preamble *pre;
	struct vb2_packed_key *k;

	memset(workbuf, 0xaa, sizeof(workbuf));

	memset(&cc, 0, sizeof(cc));
	cc.workbuf = workbuf;
	cc.workbuf_size = sizeof(workbuf);

	vb2_init_context(&cc);
	sd = vb2_get_sd(&cc);

	vb2_nv_init(&cc);

	vb2_secdata_create(&cc);
	vb2_secdata_init(&cc);

	retval_vb2_fw_parse_gbb = VB2_SUCCESS;
	retval_vb2_check_dev_switch = VB2_SUCCESS;
	retval_vb2_check_tpm_clear = VB2_SUCCESS;
	retval_vb2_select_fw_slot = VB2_SUCCESS;
	retval_vb2_verify_fw_keyblock = VB2_SUCCESS;
	retval_vb2_verify_fw_preamble2 = VB2_SUCCESS;
	retval_vb2_digest_finalize = VB2_SUCCESS;
	retval_vb2_verify_digest = VB2_SUCCESS;

	sd->workbuf_preamble_offset = 8;
	sd->workbuf_preamble_size = sizeof(*pre);
	cc.workbuf_used = sd->workbuf_preamble_offset
		+ sd->workbuf_preamble_size;
	pre = (struct vb2_fw_preamble *)
		(cc.workbuf + sd->workbuf_preamble_offset);
	pre->body_signature.data_size = mock_body_size;
	pre->body_signature.sig_size = mock_sig_size;

	sd->workbuf_data_key_offset = cc.workbuf_used;
	sd->workbuf_data_key_size = sizeof(*k) + 8;
	cc.workbuf_used = sd->workbuf_data_key_offset +
		sd->workbuf_data_key_size;
	k = (struct vb2_packed_key *)
		(cc.workbuf + sd->workbuf_data_key_offset);
	k->algorithm = mock_algorithm;

	if (t == FOR_EXTEND_HASH || t == FOR_CHECK_HASH)
		vb2api_init_hash(&cc, VB2_HASH_TAG_FW_BODY, NULL);

	if (t == FOR_CHECK_HASH)
		vb2api_extend_hash(&cc, mock_body, mock_body_size);
};

/* Mocked functions */

int vb2_fw_parse_gbb(struct vb2_context *ctx)
{
	return retval_vb2_fw_parse_gbb;
}

int vb2_check_dev_switch(struct vb2_context *ctx)
{
	return retval_vb2_check_dev_switch;
}

int vb2_check_tpm_clear(struct vb2_context *ctx)
{
	return retval_vb2_check_tpm_clear;
}

int vb2_select_fw_slot(struct vb2_context *ctx)
{
	return retval_vb2_select_fw_slot;
}

int vb2_verify_fw_keyblock(struct vb2_context *ctx)
{
	return retval_vb2_verify_fw_keyblock;
}

int vb2_verify_fw_preamble2(struct vb2_context *ctx)
{
	return retval_vb2_verify_fw_preamble2;
}

int vb2_unpack_key(struct vb2_public_key *key,
		   const uint8_t *buf,
		   uint32_t size)
{
	struct vb2_packed_key *k = (struct vb2_packed_key *)buf;

	if (size != sizeof(*k) + 8)
		return VB2_ERROR_UNPACK_KEY_SIZE;

	key->algorithm = k->algorithm;

	return VB2_SUCCESS;
}

int vb2_digest_init(struct vb2_digest_context *dc, uint32_t algorithm)
{
	if (algorithm != mock_algorithm)
		return VB2_ERROR_SHA_INIT_ALGORITHM;

	dc->algorithm = algorithm;

	return VB2_SUCCESS;
}

int vb2_digest_extend(struct vb2_digest_context *dc,
		      const uint8_t *buf,
		      uint32_t size)
{
	if (dc->algorithm != mock_algorithm)
		return VB2_ERROR_SHA_EXTEND_ALGORITHM;

	return VB2_SUCCESS;
}

int vb2_digest_finalize(struct vb2_digest_context *dc,
			uint8_t *digest,
			uint32_t digest_size)
{
	return retval_vb2_digest_finalize;
}

uint32_t vb2_rsa_sig_size(uint32_t algorithm)
{
	return mock_sig_size;
}

int vb2_verify_digest(const struct vb2_public_key *key,
		      uint8_t *sig,
		      const uint8_t *digest,
		      struct vb2_workbuf *wb)
{
	return retval_vb2_verify_digest;
}

/* Tests */

static void misc_tests(void)
{
	/* Test secdata passthru functions */
	reset_common_data(FOR_MISC);
	/* Corrupt secdata so initial check will fail */
	cc.secdata[0] ^= 0x42;
	TEST_EQ(vb2api_secdata_check(&cc), VB2_ERROR_SECDATA_CRC,
		"secdata check");
	TEST_SUCC(vb2api_secdata_create(&cc), "secdata create");
	TEST_SUCC(vb2api_secdata_check(&cc), "secdata check 2");

	/* Test fail passthru */
	reset_common_data(FOR_MISC);
	vb2api_fail(&cc, 12, 34);
	TEST_EQ(vb2_nv_get(&cc, VB2_NV_RECOVERY_REQUEST),
		12, "vb2api_fail request");
	TEST_EQ(vb2_nv_get(&cc, VB2_NV_RECOVERY_SUBCODE),
		34, "vb2api_fail subcode");
}

static void phase1_tests(void)
{
	reset_common_data(FOR_MISC);
	TEST_SUCC(vb2api_fw_phase1(&cc), "phase1 good");
	TEST_EQ(sd->recovery_reason, 0, "  not recovery");
	TEST_EQ(cc.flags & VB2_CONTEXT_RECOVERY_MODE, 0, "  recovery flag");
	TEST_EQ(cc.flags & VB2_CONTEXT_CLEAR_RAM, 0, "  clear ram flag");

	reset_common_data(FOR_MISC);
	cc.secdata[0] ^= 0x42;
	TEST_EQ(vb2api_fw_phase1(&cc),
		VB2_ERROR_API_PHASE1_RECOVERY, "phase1 secdata");
	TEST_EQ(sd->recovery_reason, VB2_RECOVERY_SECDATA_INIT, "  recovery");
	TEST_NEQ(cc.flags & VB2_CONTEXT_RECOVERY_MODE, 0, "  recovery flag");
	TEST_NEQ(cc.flags & VB2_CONTEXT_CLEAR_RAM, 0, "  clear ram flag");
}

static void phase2_tests(void)
{
	reset_common_data(FOR_MISC);
	TEST_SUCC(vb2api_fw_phase2(&cc), "phase2 good");
	TEST_EQ(cc.flags & VB2_CONTEXT_CLEAR_RAM, 0, "  clear ram flag");

	reset_common_data(FOR_MISC);
	retval_vb2_fw_parse_gbb = VB2_ERROR_GBB_MAGIC;
	TEST_EQ(vb2api_fw_phase2(&cc), VB2_ERROR_GBB_MAGIC, "phase2 gbb");
	TEST_EQ(vb2_nv_get(&cc, VB2_NV_RECOVERY_REQUEST),
		VB2_RECOVERY_GBB_HEADER, "  recovery reason");

	reset_common_data(FOR_MISC);
	retval_vb2_check_dev_switch = VB2_ERROR_MOCK;
	TEST_EQ(vb2api_fw_phase2(&cc), VB2_ERROR_MOCK, "phase2 dev switch");
	TEST_EQ(vb2_nv_get(&cc, VB2_NV_RECOVERY_REQUEST),
		VB2_RECOVERY_DEV_SWITCH, "  recovery reason");

	reset_common_data(FOR_MISC);
	cc.flags |= VB2_CONTEXT_DEVELOPER_MODE;
	TEST_SUCC(vb2api_fw_phase2(&cc), "phase1 dev");
	TEST_NEQ(cc.flags & VB2_CONTEXT_CLEAR_RAM, 0, "  clear ram flag");

	reset_common_data(FOR_MISC);
	retval_vb2_check_tpm_clear = VB2_ERROR_MOCK;
	TEST_EQ(vb2api_fw_phase2(&cc), VB2_ERROR_MOCK, "phase2 tpm clear");
	TEST_EQ(vb2_nv_get(&cc, VB2_NV_RECOVERY_REQUEST),
		VB2_RECOVERY_TPM_CLEAR_OWNER, "  recovery reason");

	reset_common_data(FOR_MISC);
	retval_vb2_select_fw_slot = VB2_ERROR_MOCK;
	TEST_EQ(vb2api_fw_phase2(&cc), VB2_ERROR_MOCK, "phase2 slot");
	TEST_EQ(vb2_nv_get(&cc, VB2_NV_RECOVERY_REQUEST),
		VB2_RECOVERY_FW_SLOT, "  recovery reason");
}

static void phase3_tests(void)
{
	reset_common_data(FOR_MISC);
	TEST_SUCC(vb2api_fw_phase3(&cc), "phase3 good");

	reset_common_data(FOR_MISC);
	retval_vb2_verify_fw_keyblock = VB2_ERROR_MOCK;
	TEST_EQ(vb2api_fw_phase3(&cc), VB2_ERROR_MOCK, "phase3 keyblock");
	TEST_EQ(vb2_nv_get(&cc, VB2_NV_RECOVERY_REQUEST),
		VB2_RECOVERY_RO_INVALID_RW, "  recovery reason");

	reset_common_data(FOR_MISC);
	retval_vb2_verify_fw_preamble2 = VB2_ERROR_MOCK;
	TEST_EQ(vb2api_fw_phase3(&cc), VB2_ERROR_MOCK, "phase3 keyblock");
	TEST_EQ(vb2_nv_get(&cc, VB2_NV_RECOVERY_REQUEST),
		VB2_RECOVERY_RO_INVALID_RW, "  recovery reason");
}

static void init_hash_tests(void)
{
	struct vb2_packed_key *k;
	int wb_used_before;
	uint32_t size;

	/* For now, all we support is body signature hash */
	reset_common_data(FOR_MISC);
	wb_used_before = cc.workbuf_used;
	TEST_SUCC(vb2api_init_hash(&cc, VB2_HASH_TAG_FW_BODY, &size),
		  "init hash good");
	TEST_EQ(sd->workbuf_hash_offset,
		(wb_used_before + (VB2_WORKBUF_ALIGN - 1)) &
		~(VB2_WORKBUF_ALIGN - 1),
		"hash context offset");
	TEST_EQ(sd->workbuf_hash_size, sizeof(struct vb2_digest_context),
		"hash context size");
	TEST_EQ(cc.workbuf_used,
		sd->workbuf_hash_offset + sd->workbuf_hash_size,
		"hash uses workbuf");
	TEST_EQ(sd->hash_tag, VB2_HASH_TAG_FW_BODY, "hash tag");
	TEST_EQ(sd->hash_remaining_size, mock_body_size, "hash remaining");

	wb_used_before = cc.workbuf_used;
	TEST_SUCC(vb2api_init_hash(&cc, VB2_HASH_TAG_FW_BODY, NULL),
		  "init hash again");
	TEST_EQ(cc.workbuf_used, wb_used_before, "init hash reuses context");

	reset_common_data(FOR_MISC);
	TEST_EQ(vb2api_init_hash(&cc, VB2_HASH_TAG_INVALID, &size),
		VB2_ERROR_API_INIT_HASH_TAG, "init hash invalid tag");

	reset_common_data(FOR_MISC);
	sd->workbuf_preamble_size = 0;
	TEST_EQ(vb2api_init_hash(&cc, VB2_HASH_TAG_FW_BODY, &size),
		VB2_ERROR_API_INIT_HASH_PREAMBLE, "init hash preamble");

	reset_common_data(FOR_MISC);
	TEST_EQ(vb2api_init_hash(&cc, VB2_HASH_TAG_FW_BODY + 1, &size),
		VB2_ERROR_API_INIT_HASH_TAG, "init hash unknown tag");

	reset_common_data(FOR_MISC);
	cc.workbuf_used =
		cc.workbuf_size - sizeof(struct vb2_digest_context) + 8;
	TEST_EQ(vb2api_init_hash(&cc, VB2_HASH_TAG_FW_BODY, &size),
		VB2_ERROR_API_INIT_HASH_WORKBUF, "init hash workbuf");

	reset_common_data(FOR_MISC);
	sd->workbuf_data_key_size = 0;
	TEST_EQ(vb2api_init_hash(&cc, VB2_HASH_TAG_FW_BODY, &size),
		VB2_ERROR_API_INIT_HASH_DATA_KEY, "init hash data key");

	reset_common_data(FOR_MISC);
	sd->workbuf_data_key_size--;
	TEST_EQ(vb2api_init_hash(&cc, VB2_HASH_TAG_FW_BODY, &size),
		VB2_ERROR_UNPACK_KEY_SIZE, "init hash data key size");

	reset_common_data(FOR_MISC);
	k = (struct vb2_packed_key *)(cc.workbuf + sd->workbuf_data_key_offset);
	k->algorithm--;
	TEST_EQ(vb2api_init_hash(&cc, VB2_HASH_TAG_FW_BODY, &size),
		VB2_ERROR_SHA_INIT_ALGORITHM, "init hash algorithm");
}

static void extend_hash_tests(void)
{
	struct vb2_digest_context *dc;

	reset_common_data(FOR_EXTEND_HASH);
	TEST_SUCC(vb2api_extend_hash(&cc, mock_body, 32),
		"hash extend good");
	TEST_EQ(sd->hash_remaining_size, mock_body_size - 32,
		"hash extend remaining");
	TEST_SUCC(vb2api_extend_hash(&cc, mock_body, mock_body_size - 32),
		"hash extend again");
	TEST_EQ(sd->hash_remaining_size, 0, "hash extend remaining 2");

	reset_common_data(FOR_EXTEND_HASH);
	sd->workbuf_hash_size = 0;
	TEST_EQ(vb2api_extend_hash(&cc, mock_body, mock_body_size),
		VB2_ERROR_API_EXTEND_HASH_WORKBUF, "hash extend no workbuf");

	reset_common_data(FOR_EXTEND_HASH);
	TEST_EQ(vb2api_extend_hash(&cc, mock_body, mock_body_size + 1),
		VB2_ERROR_API_EXTEND_HASH_SIZE, "hash extend too much");

	reset_common_data(FOR_EXTEND_HASH);
	TEST_EQ(vb2api_extend_hash(&cc, mock_body, 0),
		VB2_ERROR_API_EXTEND_HASH_SIZE, "hash extend empty");

	reset_common_data(FOR_EXTEND_HASH);
	dc = (struct vb2_digest_context *)
		(cc.workbuf + sd->workbuf_hash_offset);
	dc->algorithm++;
	TEST_EQ(vb2api_extend_hash(&cc, mock_body, mock_body_size),
		VB2_ERROR_SHA_EXTEND_ALGORITHM, "hash extend fail");
}

static void check_hash_tests(void)
{
	struct vb2_fw_preamble *pre;

	reset_common_data(FOR_CHECK_HASH);
	TEST_SUCC(vb2api_check_hash(&cc), "check hash good");

	reset_common_data(FOR_CHECK_HASH);
	sd->workbuf_preamble_size = 0;
	TEST_EQ(vb2api_check_hash(&cc),
		VB2_ERROR_API_CHECK_HASH_PREAMBLE, "check hash preamble");

	reset_common_data(FOR_CHECK_HASH);
	sd->workbuf_hash_size = 0;
	TEST_EQ(vb2api_check_hash(&cc),
		VB2_ERROR_API_CHECK_HASH_WORKBUF, "check hash no workbuf");

	reset_common_data(FOR_CHECK_HASH);
	sd->hash_remaining_size = 1;
	TEST_EQ(vb2api_check_hash(&cc),
		VB2_ERROR_API_CHECK_HASH_SIZE, "check hash size");

	reset_common_data(FOR_CHECK_HASH);
	cc.workbuf_used = cc.workbuf_size;
	TEST_EQ(vb2api_check_hash(&cc),
		VB2_ERROR_API_CHECK_HASH_WORKBUF_DIGEST, "check hash workbuf");

	reset_common_data(FOR_CHECK_HASH);
	retval_vb2_digest_finalize = VB2_ERROR_MOCK;
	TEST_EQ(vb2api_check_hash(&cc),	VB2_ERROR_MOCK, "check hash finalize");

	reset_common_data(FOR_CHECK_HASH);
	sd->hash_tag = VB2_HASH_TAG_INVALID;
	TEST_EQ(vb2api_check_hash(&cc),
		VB2_ERROR_API_CHECK_HASH_TAG, "check hash tag");

	reset_common_data(FOR_CHECK_HASH);
	sd->workbuf_data_key_size = 0;
	TEST_EQ(vb2api_check_hash(&cc),
		VB2_ERROR_API_CHECK_HASH_DATA_KEY, "check hash data key");

	reset_common_data(FOR_CHECK_HASH);
	sd->workbuf_data_key_size--;
	TEST_EQ(vb2api_check_hash(&cc),
		VB2_ERROR_UNPACK_KEY_SIZE, "check hash data key size");

	reset_common_data(FOR_CHECK_HASH);
	pre = (struct vb2_fw_preamble *)
		(cc.workbuf + sd->workbuf_preamble_offset);
	pre->body_signature.sig_size++;
	TEST_EQ(vb2api_check_hash(&cc),
		VB2_ERROR_API_CHECK_HASH_SIG_SIZE, "check hash sig size");

	reset_common_data(FOR_CHECK_HASH);
	retval_vb2_digest_finalize = VB2_ERROR_RSA_VERIFY_DIGEST;
	TEST_EQ(vb2api_check_hash(&cc),
		VB2_ERROR_RSA_VERIFY_DIGEST, "check hash finalize");
}

int main(int argc, char* argv[])
{
	misc_tests();
	phase1_tests();
	phase2_tests();
	phase3_tests();
	init_hash_tests();
	extend_hash_tests();
	check_hash_tests();

	return gTestSuccess ? 0 : 255;
}
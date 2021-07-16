/*
 * aws.h for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013-2014, Grasshopper
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is aws.h for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Grasshopper
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Rienzo <chris.rienzo@grasshopper.com>
 * Quoc-Bao Nguyen <baonq5@vng.com.vn>
 * 
 * test_aws.c - Unit tests for functions in aws.c
 *
 */

#include <switch.h>
#include <test/switch_test.h>
#include "../gcs.c"
#include <stdlib.h>

// Run test
// make && libtool --mode=execute valgrind --leak-check=full  --log-file=vg.log ./test/test_aws && cat vg.log

FST_BEGIN()
{

FST_SUITE_BEGIN()
{

FST_SETUP_BEGIN()
{
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()

#if defined(HAVE_OPENSSL)
FST_TEST_BEGIN(encoded_token)
{
//char *encoded_token(const char *token_uri, const char *client_email, const char *private_key_id, int *token_length, time_t now) {
	time_t now = 1615402513;
	char *token = NULL;
	int token_length;
    token = encoded_token("https://accounts.google.com/o/oauth2/token", "gcs@freeswitch.com", "667265657377697463682D676373", &token_length, now);
	fst_check_string_equals("eyJ0eXAiOiJKV1QiLCJhbGciOiJSUzI1NiIsImtpZCI6IjY2NzI2NTY1NzM3NzY5NzQ2MzY4MkQ2NzYzNzMifQ.eyJpYXQiOiIxNjE1NDAyNTEzIiwiZXhwIjoiMTYxNTQwNjExMyIsImlzcyI6Imdjc0BmcmVlc3dpdGNoLmNvbSIsImF1ZCI6Imh0dHBzOi8vYWNjb3VudHMuZ29vZ2xlLmNvbS9vL29hdXRoMi90b2tlbiIsInNjb3BlIjoiaHR0cHM6Ly93d3cuZ29vZ2xlYXBpcy5jb20vYXV0aC9kZXZzdG9yYWdlLmZ1bGxfY29udHJvbCBodHRwczovL3d3dy5nb29nbGVhcGlzLmNvbS9hdXRoL2RldnN0b3JhZ2UucmVhZF9vbmx5IGh0dHBzOi8vd3d3Lmdvb2dsZWFwaXMuY29tL2F1dGgvZGV2c3RvcmFnZS5yZWFkX3dyaXRlIn0", token);
	free(token);
}
FST_TEST_END()

FST_TEST_BEGIN(signtoken)
{
//void signtoken(char *token, int tokenlen,char *pkey, char *out) {
	char *encoded = NULL;
	char *pkey = "-----BEGIN PRIVATE KEY-----\nMIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCXiVOV3h61llym\nnpHamUHsuVjrdDiEQnNX1KA4k/kcfP+gqRjL9m3YAxXfUA9GFCIC2OYvdciW3Ggm\nt4CSYmSltljKjbN2JHs7iNQw4CcFfiAXhxL0TYNhNE9wBDOZRsC1Uusv38RPwqwd\n922pAGzF+PqNE2j+zSxlNOnlJfyJDMKrqCGV8CclS+j/u41MaT6cpOlHP6KgaS01\nJJVyaqLpnMLWAx9/5G6Y/YWah+obEyn7yoDva/1Yhlnq20CMHlh3ifDfYYrS9rtp\nhdutz4fESKFPKymlG9aGfFuCME3GP8Rn8ZdPbsAWZ2cf388CLjlxEid1EU8klr2X\n+G1+3di/AgMBAAECggEAHL6UZ9+/5IMWpRZ8JUKgAjbwWo1rsQ7n0TfIgqLzBIfj\nd4bL6NigYnLHYdpOY2UrRG3/T+5gM9mwOfPiBCJ85AAwXI+/hIAMDjF4yqKiVETl\n8oCRRF01uCkTjnSFkyQcJukJKsYf918+hdqq5v1pJK6DXGJbrsWdj78XRPvNKPPD\naEEvSNwdet12Jz9wkmj2g7zg8KMX18v3IUyf7HKjb/cjomB0WuIDfJchDRYlxrfJ\n4DShcIfMi+04C/FFN+vbP77tXQM7O3Z81uqDQAO3k3NoXTTNBZGLP+SOyDUSRZQS\nCNb3J6cwTC737e0M6K+zVP1f03ynuU2u+dHiVVMTtQKBgQDNw/19HSxoJ/5nGHbD\nLW1g33Jm4Nr05lYnetEPS0wkruzEbTy3B6prl34KUslreuUTAuhtFxCLsEgT/sMO\nrxX/OM7RaNUILrpLrzen/eVNeiquM1wLEI52VNkRU5GlTZEJohGE+a3YP+kQTLe5\n8xmzKfJUllyfpXGxDNkryjaeSwKBgQC8iBqDJs6h9tkdxC/XC8+qSJ+WBk5tr3PM\nyx/x1NGKO5TpgdhRr98GYYUhoph+TIp0/8/+d2lVDzO4SAOzms3xnANPEzJcLi7c\nCa8ECOW3S4HhWE61QsbVY5xA83hGAO2WQN22vu9KwhyFU145aSQH0tJrQoevkdBl\ndpqtP15W3QKBgQCsL8gePJ1+g4k2SJiJd6hCGnoncR6JNX7/Bp2PiNktEVx8e1UF\nbNrFsj388Y4v7OVo5VQOhfCIlHmckeI0lXt42dboEivC7ydiUjvmzmZmUUcKA1yQ\nvcgZaaNEBoSoqaInR4IVnsJFZiXoR+qvJqlo7j8lXbYgulfLaw8Iv+y4xQKBgGK6\no6eq2urWajy8UJE9DjMOdQQLqWanSu0kMkZiPJk3OnROGwosH48n4qAKlfEOBDPh\nAvsvbWmt3FfU3ptfphmwqcrvMqAzTzbLm2txfVrPn+RyakViAt4cm+cnmQSP19un\nfHQG6SktHeJ0FhPai5PNQ4QIAyZeJdP8mGPBm5XBAoGBAJWnXiapFMcHi3DgsVb4\n+ni8Qvs293OvsdjHlo2eent/Kwbrdytw/V8uhq9awJb1npdgVd54RrbZ+Jq4x19K\nt06Jz9/EAoLLfL+tqzpEiKvSLjdKpedPm1Cgfj0KTM+MqoSU0bv4gMssnM428luJ\nv5ptWjeaHoYpJzvGfGBouVCI\n-----END PRIVATE KEY-----\n";
	int tlength =  1 + snprintf(NULL, 0, "%s", pkey);
	encoded = malloc(sizeof(char) * 343);
	signtoken("eyJ0eXAiOiJKV1QiLCJhbGciOiJSUzI1NiIsImtpZCI6IjY2NzI2NTY1NzM3NzY5NzQ2MzY4MkQ2NzYzNzMifQ.eyJpYXQiOiIxNjE1NDAyNTEzIiwiZXhwIjoiMTYxNTQwNjExMyIsImlzcyI6Imdjc0BmcmVlc3dpdGNoLmNvbSIsImF1ZCI6Imh0dHBzOi8vYWNjb3VudHMuZ29vZ2xlLmNvbS9vL29hdXRoMi90b2tlbiIsInNjb3BlIjoiaHR0cHM6Ly93d3cuZ29vZ2xlYXBpcy5jb20vYXV0aC9kZXZzdG9yYWdlLmZ1bGxfY29udHJvbCBodHRwczovL3d3dy5nb29nbGVhcGlzLmNvbS9hdXRoL2RldnN0b3JhZ2UucmVhZF9vbmx5IGh0dHBzOi8vd3d3Lmdvb2dsZWFwaXMuY29tL2F1dGgvZGV2c3RvcmFnZS5yZWFkX3dyaXRlIn0", tlength, pkey, encoded);
	fst_check_string_equals("iSNfnz8gn1q8cYUrs7+m3hhXRalFRemt9SXvlnrzr1tyYx2ztL0m/882jghpsHMDVcti59avBjamcbrwMRDah7KXomu2cuCEwuYaGo6C7KTe9WZf+J0ep2shOAJJwnDKvUfuyVT21EEf/rWXfy8lblsq/YK0D7982FsXUgPDFU5NzPiw2fWk6YEPWbET3Yy+gJiTqYj5P0Fo1s66LNrLIX1RbH6/GW7d+lDl2RNMCZxGTtEygLdAXj+l0OPQG4Qvfzeymi9zWEq6j0rAChT0OppjZKqWf9IjMRbPUbuwIpNmgntfU7OcYtTFeiLy+W9fFutXEbSrq72MxMOrDEWWhQ", encoded);
	free(encoded);
}
FST_TEST_END()

FST_TEST_BEGIN(parse_xml_config_with_gcs)
{
	switch_xml_t cfg, profiles, profile, gcs_profile;
	http_profile_t http_profile;
	int fd;
	int i = 0;


	fd = open("test_gcs_http_cache.conf.xml", O_RDONLY);
	if (fd < 0) {
		//printf("Open in test dir\n");
		fd = open("test/parse_xml_config_with_gcs", O_RDONLY);
	}
	fst_check(fd > 0);

	cfg = switch_xml_parse_fd(fd);
	fst_check(cfg != NULL);

	profiles = switch_xml_child(cfg, "profiles");
	fst_check(profiles);

	for (profile = switch_xml_child(profiles, "profile"); profile; profile = profile->next) {
		const char *name = NULL;
		switch_memory_pool_t *pool;
		switch_core_new_memory_pool(&pool);
		i++;

		fst_check(profile);

		name = switch_xml_attr_soft(profile, "name");
		fst_check(name);

		http_profile.name = name;
		http_profile.aws_s3_access_key_id = NULL;
		http_profile.secret_access_key = NULL;
		http_profile.base_domain = NULL;
		http_profile.region = NULL;

		gcs_profile = switch_xml_child(profile, "gcs");
		gcs_config_profile(gcs_profile, &http_profile, pool);
//aws_s3_access_key_id, secret_access_key, gcs_email, region
		fst_check(!zstr(http_profile.region));
		fst_check(!zstr(http_profile.aws_s3_access_key_id));
		fst_check(!zstr(http_profile.secret_access_key));
		switch_safe_free(http_profile.region);
		switch_safe_free(http_profile.aws_s3_access_key_id);
		switch_safe_free(http_profile.secret_access_key);
		switch_safe_free(http_profile.base_domain);
	}

	fst_check(i == 1);      // test data contain two config

	switch_xml_free(cfg);
}
FST_TEST_END()

#endif

}
FST_SUITE_END()

}
FST_END()

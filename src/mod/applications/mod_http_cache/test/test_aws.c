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
#include "../aws.c"

// Run test
// make && libtool --mode=execute valgrind --leak-check=full  --log-file=vg.log ./test/test_aws && cat vg.log

FST_BEGIN()
{

FST_SUITE_BEGIN(aws)
{

#if defined(HAVE_OPENSSL)
	char url[100] = {'\0'};
	switch_aws_s3_profile aws_s3_profile;

	// Get bucket and object name from url
	aws_s3_profile.bucket = "bucket6";
	aws_s3_profile.object = "document.docx";
	memcpy(aws_s3_profile.date_stamp, "20190729", DATE_STAMP_LENGTH);
	memcpy(aws_s3_profile.time_stamp, "20190729T083832Z", TIME_STAMP_LENGTH);
	aws_s3_profile.access_key_id = "cbc443a53fb06eafb2b83ca1e4233cbc";
	aws_s3_profile.access_key_secret = "4a722120f27518abbb8573ca9005d175";

	aws_s3_profile.base_domain = "stg.vinadata.vn";
	aws_s3_profile.region = "HCM";
	aws_s3_profile.verb = "GET";
	aws_s3_profile.expires = DEFAULT_EXPIRATION_TIME;

	switch_snprintf(url, sizeof(url), "http://%s.%s/%s", aws_s3_profile.bucket, aws_s3_profile.base_domain, aws_s3_profile.object);
#endif

FST_SETUP_BEGIN()
{
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()

#if defined(HAVE_OPENSSL)
FST_TEST_BEGIN(parse_url)
{
	char *bucket;
	char *object;
	char url_dup[512] = { 0 };

	switch_snprintf(url_dup, sizeof(url_dup), url);
	parse_url(url_dup, aws_s3_profile.base_domain, "s3", &bucket, &object);
	fst_check_string_equals(aws_s3_profile.bucket, bucket);
	fst_check_string_equals(aws_s3_profile.object, object);

	switch_snprintf(url_dup, sizeof(url_dup), "https://bucket99.s3.amazonaws.com/image.png");
	parse_url(url_dup, NULL, "s3", &bucket, &object);
	fst_check_string_equals(bucket, "bucket99");
	fst_check_string_equals(object, "image.png");

	switch_snprintf(url_dup, sizeof(url_dup), "https://bucket99.s3.amazonaws.com/folder5/image.png");
	parse_url(url_dup, NULL, "s3", &bucket, &object);
	fst_check_string_equals(bucket, "bucket99");
	fst_check_string_equals(object, "folder5/image.png");

	switch_snprintf(url_dup, sizeof(url_dup), "https://bucket23.vn-hcm.vinadata.vn/image.png");
	parse_url(url_dup, "vn-hcm.vinadata.vn", "s3", &bucket, &object);
	fst_check_string_equals(bucket, "bucket23");
	fst_check_string_equals(object, "image.png");

	switch_snprintf(url_dup, sizeof(url_dup), "https://bucket335.s3-ap-southeast-1.amazonaws.com/vpnclient-v4.29-9680-rtm-2019.02.28-linux-x64-64bit.tar.gz");
	parse_url(url_dup, NULL, "s3", &bucket, &object);
	fst_check_string_equals(bucket, "bucket335");
	fst_check_string_equals(object, "vpnclient-v4.29-9680-rtm-2019.02.28-linux-x64-64bit.tar.gz");

	switch_snprintf(url_dup, sizeof(url_dup), "https://bucket335.s3-ap-southeast-1.amazonaws.com/vpnclient-v4.29-9680-rtm-2019.02.28-linux-x64-64bit.tar.gz");
	parse_url(url_dup, "s3-ap-southeast-1.amazonaws.com", "s3", &bucket, &object);
	fst_check_string_equals(bucket, "bucket335");
	fst_check_string_equals(object, "vpnclient-v4.29-9680-rtm-2019.02.28-linux-x64-64bit.tar.gz");

	switch_snprintf(url_dup, sizeof(url_dup), "http://quotes.s3.amazonaws.com/nelson");
	parse_url(url_dup, NULL, "s3", &bucket, &object);
	fst_check_string_equals("quotes", bucket);
	fst_check_string_equals("nelson", object);

	switch_snprintf(url_dup, sizeof(url_dup), "https://quotes.s3.amazonaws.com/nelson.mp3");
	parse_url(url_dup, NULL, "s3", &bucket, &object);
	fst_check_string_equals("quotes", bucket);
	fst_check_string_equals("nelson.mp3", object);

	switch_snprintf(url_dup, sizeof(url_dup), "http://s3.amazonaws.com/quotes/nelson");
	parse_url(url_dup, NULL, "s3", &bucket, &object);
	fst_check(bucket == NULL);
	fst_check(object == NULL);

	switch_snprintf(url_dup, sizeof(url_dup), "http://quotes/quotes/nelson");
	parse_url(url_dup, NULL, "s3", &bucket, &object);
	fst_check(bucket == NULL);
	fst_check(object == NULL);

	switch_snprintf(url_dup, sizeof(url_dup), "http://quotes.s3.amazonaws.com/");
	parse_url(url_dup, NULL, "s3", &bucket, &object);
	fst_check(bucket == NULL);
	fst_check(object == NULL);

	switch_snprintf(url_dup, sizeof(url_dup), "http://quotes.s3.amazonaws.com");
	parse_url(url_dup, NULL, "s3", &bucket, &object);
	fst_check(bucket == NULL);
	fst_check(object == NULL);

	switch_snprintf(url_dup, sizeof(url_dup), "http://quotes");
	parse_url(url_dup, NULL, "s3", &bucket, &object);
	fst_check(bucket == NULL);
	fst_check(object == NULL);

	switch_snprintf(url_dup, sizeof(url_dup), "%s", "");
	parse_url(url_dup, NULL, "s3", &bucket, &object);
	fst_check(bucket == NULL);
	fst_check(object == NULL);

	switch_snprintf(NULL, 0, "s3", &bucket, &object);
	fst_check(bucket == NULL);
	fst_check(object == NULL);

	switch_snprintf(url_dup, sizeof(url_dup), "http://bucket.s3.amazonaws.com/voicemails/recording.wav");
	parse_url(url_dup, NULL, "s3", &bucket, &object);
	fst_check_string_equals("bucket", bucket);
	fst_check_string_equals("voicemails/recording.wav", object);

	switch_snprintf(url_dup, sizeof(url_dup), "https://my-bucket-with-dash.s3-us-west-2.amazonaws.com/greeting/file/1002/Lumino.mp3");
	parse_url(url_dup, NULL, "s3", &bucket, &object);
	fst_check_string_equals("my-bucket-with-dash", bucket);
	fst_check_string_equals("greeting/file/1002/Lumino.mp3", object);

	switch_snprintf(url_dup, sizeof(url_dup), "http://quotes.s3.foo.bar.s3.amazonaws.com/greeting/file/1002/Lumino.mp3");
	parse_url(url_dup, NULL, "s3", &bucket, &object);
	fst_check_string_equals("quotes.s3.foo.bar", bucket);
	fst_check_string_equals("greeting/file/1002/Lumino.mp3", object);

	switch_snprintf(url_dup, sizeof(url_dup), "http://quotes.s3.foo.bar.example.com/greeting/file/1002/Lumino.mp3");
	parse_url(url_dup, "example.com", "s3", &bucket, &object);
	fst_check_string_equals("quotes.s3.foo.bar", bucket);
	fst_check_string_equals("greeting/file/1002/Lumino.mp3", object);
}
FST_TEST_END()

FST_TEST_BEGIN(aws_s3_standardized_query_string)
{
	char* standardized_query_string = aws_s3_standardized_query_string(&aws_s3_profile);
	fst_check_string_equals("X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=cbc443a53fb06eafb2b83ca1e4233cbc%2F20190729%2FHCM%2Fs3%2Faws4_request&X-Amz-Date=20190729T083832Z&X-Amz-Expires=604800&X-Amz-SignedHeaders=host", standardized_query_string);
	switch_safe_free(standardized_query_string);
}
FST_TEST_END()

FST_TEST_BEGIN(get_time)
{
	char time_stamp[TIME_STAMP_LENGTH];
	char date_stamp[DATE_STAMP_LENGTH];
	char time_stamp_test[TIME_STAMP_LENGTH];
	char date_stamp_test[DATE_STAMP_LENGTH];

	// Get date and time for test case
	time_t rawtime;
	struct tm * timeinfo;
	time(&rawtime);
	timeinfo = gmtime(&rawtime);

	// Get date and time to test
	get_time("%Y%m%d", date_stamp, DATE_STAMP_LENGTH);
	get_time("%Y%m%dT%H%M%SZ", time_stamp, TIME_STAMP_LENGTH);

	// https://fresh2refresh.com/c-programming/c-time-related-functions/
	// https://stackoverflow.com/questions/5141960/get-the-current-time-in-c/5142028
	// https://linux.die.net/man/3/ctime
	// https://stackoverflow.com/questions/153890/printing-leading-0s-in-c
	switch_snprintf(date_stamp_test, DATE_STAMP_LENGTH, "%d%02d%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday);
	switch_snprintf(time_stamp_test, TIME_STAMP_LENGTH, "%d%02d%02dT%02d%02d%02dZ", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

	fst_check_string_equals(time_stamp_test, time_stamp);
	fst_check_string_equals(date_stamp_test, date_stamp);
}
FST_TEST_END()

FST_TEST_BEGIN(hmac256_hex)
{
	char hex[SHA256_DIGEST_LENGTH * 2 + 1];

	fst_check_string_equals("61d8c60f9c2cd767d3db37a52966965ef508136693d99ea533cff1b712044653", hmac256_hex(hex, "d8a1c4f68b15844de5d07960a57b1669", SHA256_DIGEST_LENGTH, "27a7d569d0c12cc576f20665651fb72c"));
	fst_check_string_equals("5a98f20477a538bd29f0903cc30accaf4151b22e1f44577b75bae4cc5068df9e", hmac256_hex(hex, "66b0d6c5b3fd9c57a345b03877c902cb", SHA256_DIGEST_LENGTH, "2da091ff2a9818ce6deb5c4b6d9ad51c"));
	fst_check_string_equals("6accbbef08f240dbdebf154cda91f7c66ef178023d53db7f3656d204996effaa", hmac256_hex(hex, "820f6b29b5ca8fa1077b69edf4ee456f", SHA256_DIGEST_LENGTH, "063ee28c963df34342ffb7ac0feae1d9"));
}
FST_TEST_END()

FST_TEST_BEGIN(sha256_hex)
{
	char hex[SHA256_DIGEST_LENGTH * 2 + 1];

	fst_check_string_equals("ebab701faffb9cd018d7fa566ca0e7f55dd7a9850cae06e088554238d6fae257", sha256_hex(hex, "eccbb6195a0f08664e2a35c0d686e892"));
	fst_check_string_equals("4884c0be257758ded0381f940870a9280b367002e5c518fb42d56641b451a66b", sha256_hex(hex, "1993f63438fe482cd3040aeb2390b98c"));
	fst_check_string_equals("1c930bd8e5034a418fef94b1cb753ec82b2a510429bfcdf41b597c6f6c7b21e4", sha256_hex(hex, "3705c5709dc52a04d844ebbcf59e7672"));
}
FST_TEST_END()

FST_TEST_BEGIN(aws_s3_standardized_request)
{
	char* aws_s3_standardized_request_str = aws_s3_standardized_request(&aws_s3_profile);

	fst_check_string_equals("GET\n/document.docx\nX-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=cbc443a53fb06eafb2b83ca1e4233cbc%2F20190729%2FHCM%2Fs3%2Faws4_request&X-Amz-Date=20190729T083832Z&X-Amz-Expires=604800&X-Amz-SignedHeaders=host\nhost:bucket6.stg.vinadata.vn\n\nhost\nUNSIGNED-PAYLOAD", aws_s3_standardized_request_str);
	switch_safe_free(aws_s3_standardized_request_str);
}
FST_TEST_END()

FST_TEST_BEGIN(aws_s3_string_to_sign)
{
	char* aws_s3_standardized_request_str = aws_s3_standardized_request(&aws_s3_profile);
	char* aws_s3_string_to_sign_str = aws_s3_string_to_sign(aws_s3_standardized_request_str, &aws_s3_profile);

	fst_check_string_equals("AWS4-HMAC-SHA256\n20190729T083832Z\n20190729/HCM/s3/aws4_request\n945cd2782c8685f5b2472873252fa048eaa37cf8b132ef667bd98b6ad33238ac", aws_s3_string_to_sign_str);

	switch_safe_free(aws_s3_standardized_request_str);
	switch_safe_free(aws_s3_string_to_sign_str);
}
FST_TEST_END()

FST_TEST_BEGIN(aws_s3_signature_key)
{
	char signature_key[SHA256_DIGEST_LENGTH];
	unsigned int aws_s3_signature_key_b64_size = SHA256_DIGEST_LENGTH * 4 / 3 + 5;
	unsigned char* aws_s3_signature_key_b64 = (unsigned char*)malloc(aws_s3_signature_key_b64_size);
	char* aws_s3_signature_key_buffer = aws_s3_signature_key(signature_key, &aws_s3_profile);

	switch_b64_encode((unsigned char*)aws_s3_signature_key_buffer, SHA256_DIGEST_LENGTH, aws_s3_signature_key_b64, aws_s3_signature_key_b64_size);
	fst_check_string_equals("2TBIZBxK1k+qh/pvEs0d2iNQ4SSX63o/8pLzzFPeA7c=", (char*)aws_s3_signature_key_b64);

	switch_safe_free(aws_s3_signature_key_b64);
}
FST_TEST_END()

FST_TEST_BEGIN(aws_s3_authentication_create)
{
	char* query_param = aws_s3_authentication_create(&aws_s3_profile);

	fst_check_string_equals("X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=cbc443a53fb06eafb2b83ca1e4233cbc%2F20190729%2FHCM%2Fs3%2Faws4_request&X-Amz-Date=20190729T083832Z&X-Amz-Expires=604800&X-Amz-SignedHeaders=host&X-Amz-Signature=3d0e5c18e85440a6cd38bdf8b3d07476fe6f98b8456a39ec401d1c628ce19175", query_param);

	switch_safe_free(query_param);
}
FST_TEST_END()

FST_TEST_BEGIN(parse_xml_config_with_aws)
{
	switch_xml_t cfg, profiles, profile, aws_s3_profile;
	http_profile_t http_profile;
	int fd;
	int i = 0;

	printf("\n");

	fd = open("test_aws_http_cache.conf.xml", O_RDONLY);
	if (fd < 0) {
		fd = open("test/test_aws_http_cache.conf.xml", O_RDONLY);
	}
	fst_check(fd > 0);

	cfg = switch_xml_parse_fd(fd);
	fst_check(cfg != NULL);

	profiles = switch_xml_child(cfg, "profiles");
	fst_check(profiles);

	for (profile = switch_xml_child(profiles, "profile"); profile; profile = profile->next) {
		const char *name = NULL;
		i++;

		fst_check(profile);

		name = switch_xml_attr_soft(profile, "name");
		printf("testing profile name: %s\n", name);
		fst_check(name);

		http_profile.name = name;
		http_profile.aws_s3_access_key_id = NULL;
		http_profile.secret_access_key = NULL;
		http_profile.base_domain = NULL;
		http_profile.region = NULL;
		http_profile.append_headers_ptr = NULL;

		aws_s3_profile = switch_xml_child(profile, "aws-s3");
		fst_check(aws_s3_profile);

		fst_check(aws_s3_config_profile(aws_s3_profile, &http_profile) == SWITCH_STATUS_SUCCESS);

		fst_check(!zstr(http_profile.region));
		fst_check(!zstr(http_profile.aws_s3_access_key_id));
		fst_check(!zstr(http_profile.secret_access_key));
		printf("base domain: %s\n", http_profile.base_domain);
		fst_check(!zstr(http_profile.base_domain));
		switch_safe_free(http_profile.region);
		switch_safe_free(http_profile.aws_s3_access_key_id);
		switch_safe_free(http_profile.secret_access_key);
		switch_safe_free(http_profile.base_domain);
	}

	fst_check(i == 2);      // test data contain two config

	switch_xml_free(cfg);
}
FST_TEST_END()
#endif

}
FST_SUITE_END()

}
FST_END()

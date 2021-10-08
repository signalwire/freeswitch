
#include <switch.h>
#include <test/switch_test.h>
#include "../aws.c"

FST_BEGIN()
{
FST_SUITE_BEGIN(aws)
{
FST_SETUP_BEGIN()
{
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()

FST_TEST_BEGIN(test_string_to_sign)
{
	char *string_to_sign = NULL;
	string_to_sign = aws_s3_string_to_sign("GET", "rienzo-vault", "troporocks.mp3", "", "", "Fri, 17 May 2013 19:35:26 GMT")	;
	fst_check_string_equals("GET\n\n\nFri, 17 May 2013 19:35:26 GMT\n/rienzo-vault/troporocks.mp3", string_to_sign);
	switch_safe_free(string_to_sign);

	string_to_sign = aws_s3_string_to_sign("GET", "foo", "man.chu", "audio/mpeg", "c8fdb181845a4ca6b8fec737b3581d76", "Thu, 17 Nov 2005 18:49:58 GMT");
	fst_check_string_equals("GET\nc8fdb181845a4ca6b8fec737b3581d76\naudio/mpeg\nThu, 17 Nov 2005 18:49:58 GMT\n/foo/man.chu", string_to_sign);
	switch_safe_free(string_to_sign);

	string_to_sign = aws_s3_string_to_sign("", "", "", "", "", "");
	fst_check_string_equals("\n\n\n\n//", string_to_sign);
	switch_safe_free(string_to_sign);

	string_to_sign = aws_s3_string_to_sign(NULL, NULL, NULL, NULL, NULL, NULL);
	fst_check_string_equals("\n\n\n\n//", string_to_sign);
	switch_safe_free(string_to_sign);

	string_to_sign = aws_s3_string_to_sign("PUT", "bucket", "voicemails/recording.wav", "audio/wav", "", "Wed, 12 Jun 2013 13:16:58 GMT");
	fst_check_string_equals("PUT\n\naudio/wav\nWed, 12 Jun 2013 13:16:58 GMT\n/bucket/voicemails/recording.wav", string_to_sign);
	switch_safe_free(string_to_sign);
}
FST_TEST_END()

FST_TEST_BEGIN(test_signature)
{
	char signature[S3_SIGNATURE_LENGTH_MAX];
	signature[0] = '\0';
	fst_check_string_equals("weGrLrc9HDlkYPTepVl0A9VYNlw=", aws_s3_signature(signature, S3_SIGNATURE_LENGTH_MAX, "GET\n\n\nFri, 17 May 2013 19:35:26 GMT\n/rienzo-vault/troporocks.mp3", "hOIZt1oeTX1JzINOMBoKf0BxONRZNQT1J8gIznLx"));
	fst_check_string_equals("jZNOcbfWmD/A/f3hSvVzXZjM2HU=", aws_s3_signature(signature, S3_SIGNATURE_LENGTH_MAX, "PUT\nc8fdb181845a4ca6b8fec737b3581d76\ntext/html\nThu, 17 Nov 2005 18:49:58 GMT\nx-amz-magic:abracadabra\nx-amz-meta-author:foo@bar.com\n/quotes/nelson", "OtxrzxIsfpFjA7SwPzILwy8Bw21TLhquhboDYROV"));
	fst_check_string_equals("5m+HAmc5JsrgyDelh9+a2dNrzN8=", aws_s3_signature(signature, S3_SIGNATURE_LENGTH_MAX, "GET\n\n\n\nx-amz-date:Thu, 17 Nov 2005 18:49:58 GMT\nx-amz-magic:abracadabra\n/quotes/nelson", "OtxrzxIsfpFjA7SwPzILwy8Bw21TLhquhboDYROV"));
	fst_check_string_equals("OKA87rVp3c4kd59t8D3diFmTfuo=", aws_s3_signature(signature, S3_SIGNATURE_LENGTH_MAX, "", "OtxrzxIsfpFjA7SwPzILwy8Bw21TLhquhboDYROV"));
	fst_check_string_equals("OKA87rVp3c4kd59t8D3diFmTfuo=", aws_s3_signature(signature, S3_SIGNATURE_LENGTH_MAX, NULL, "OtxrzxIsfpFjA7SwPzILwy8Bw21TLhquhboDYROV"));
	fst_check(aws_s3_signature(signature, S3_SIGNATURE_LENGTH_MAX, "GET\n\n\n\nx-amz-date:Thu, 17 Nov 2005 18:49:58 GMT\nx-amz-magic:abracadabra\n/quotes/nelson", "") == NULL);
	fst_check(aws_s3_signature(signature, S3_SIGNATURE_LENGTH_MAX, "", "") == NULL);
	fst_check(aws_s3_signature(signature, S3_SIGNATURE_LENGTH_MAX, NULL, NULL) == NULL);
	fst_check(aws_s3_signature(NULL, S3_SIGNATURE_LENGTH_MAX, "PUT\nc8fdb181845a4ca6b8fec737b3581d76\ntext/html\nThu, 17 Nov 2005 18:49:58 GMT\nx-amz-magic:abracadabra\nx-amz-meta-author:foo@bar.com\n/quotes/nelson", "OtxrzxIsfpFjA7SwPzILwy8Bw21TLhquhboDYROV") == NULL);
	fst_check(aws_s3_signature(signature, 0, "PUT\nc8fdb181845a4ca6b8fec737b3581d76\ntext/html\nThu, 17 Nov 2005 18:49:58 GMT\nx-amz-magic:abracadabra\nx-amz-meta-author:foo@bar.com\n/quotes/nelson", "OtxrzxIsfpFjA7SwPzILwy8Bw21TLhquhboDYROV") == NULL);
	fst_check_string_equals("jZNO", aws_s3_signature(signature, 5, "PUT\nc8fdb181845a4ca6b8fec737b3581d76\ntext/html\nThu, 17 Nov 2005 18:49:58 GMT\nx-amz-magic:abracadabra\nx-amz-meta-author:foo@bar.com\n/quotes/nelson", "OtxrzxIsfpFjA7SwPzILwy8Bw21TLhquhboDYROV"));
}
FST_TEST_END()

FST_TEST_BEGIN(test_parse_url)
{
	char *bucket;
	char *object;
	char url[512] = { 0 };

	snprintf(url, sizeof(url), "http://quotes.s3.amazonaws.com/nelson");
	parse_url(url, NULL, "s3", &bucket, &object);
	fst_check_string_equals("quotes", bucket);
	fst_check_string_equals("nelson", object);

	snprintf(url, sizeof(url), "https://quotes.s3.amazonaws.com/nelson.mp3");
	parse_url(url, NULL, "s3", &bucket, &object);
	fst_check_string_equals("quotes", bucket);
	fst_check_string_equals("nelson.mp3", object);

	snprintf(url, sizeof(url), "http://s3.amazonaws.com/quotes/nelson");
	parse_url(url, NULL, "s3", &bucket, &object);
	fst_check(bucket == NULL);
	fst_check(object == NULL);

	snprintf(url, sizeof(url), "http://quotes/quotes/nelson");
	parse_url(url, NULL, "s3", &bucket, &object);
	fst_check(bucket == NULL);
	fst_check(object == NULL);

	snprintf(url, sizeof(url), "http://quotes.s3.amazonaws.com/");
	parse_url(url, NULL, "s3", &bucket, &object);
	fst_check(bucket == NULL);
	fst_check(object == NULL);

	snprintf(url, sizeof(url), "http://quotes.s3.amazonaws.com");
	parse_url(url, NULL, "s3", &bucket, &object);
	fst_check(bucket == NULL);
	fst_check(object == NULL);

	snprintf(url, sizeof(url), "http://quotes");
	parse_url(url, NULL, "s3", &bucket, &object);
	fst_check(bucket == NULL);
	fst_check(object == NULL);

	snprintf(url, sizeof(url), "%s", "");
	parse_url(url, NULL, "s3", &bucket, &object);
	fst_check(bucket == NULL);
	fst_check(object == NULL);

	parse_url(NULL, NULL, "s3", &bucket, &object);
	fst_check(bucket == NULL);
	fst_check(object == NULL);

	snprintf(url, sizeof(url), "http://bucket.s3.amazonaws.com/voicemails/recording.wav");
	parse_url(url, NULL, "s3", &bucket, &object);
	fst_check_string_equals("bucket", bucket);
	fst_check_string_equals("voicemails/recording.wav", object);

	snprintf(url, sizeof(url), "https://my-bucket-with-dash.s3-us-west-2.amazonaws.com/greeting/file/1002/Lumino.mp3");
	parse_url(url, NULL, "s3", &bucket, &object);
	fst_check_string_equals("my-bucket-with-dash", bucket);
	fst_check_string_equals("greeting/file/1002/Lumino.mp3", object);

	snprintf(url, sizeof(url), "http://quotes.s3.foo.bar.s3.amazonaws.com/greeting/file/1002/Lumino.mp3");
	parse_url(url, NULL, "s3", &bucket, &object);
	fst_check_string_equals("quotes.s3.foo.bar", bucket);
	fst_check_string_equals("greeting/file/1002/Lumino.mp3", object);

	snprintf(url, sizeof(url), "http://quotes.s3.foo.bar.example.com/greeting/file/1002/Lumino.mp3");
	parse_url(url, "example.com", "s3", &bucket, &object);
	fst_check_string_equals("quotes.s3.foo.bar", bucket);
	fst_check_string_equals("greeting/file/1002/Lumino.mp3", object);
}
FST_TEST_END()

FST_TEST_BEGIN(test_authorization_header)
{
	char *authentication_header = aws_s3_authentication_create("GET", "https://vault.s3.amazonaws.com/awesome.mp3", NULL, "audio/mpeg", "", "AKIAIOSFODNN7EXAMPLE", "0123456789012345678901234567890123456789", "1234567890");
	fst_check_string_equals("AWS AKIAIOSFODNN7EXAMPLE:YJkomOaqUJlvEluDq4fpusID38Y=", authentication_header);
	switch_safe_free(authentication_header);

	authentication_header = aws_s3_authentication_create("GET", "https://vault.s3.amazonaws.com/awesome.mp3", "s3.amazonaws.com", "audio/mpeg", "", "AKIAIOSFODNN7EXAMPLE", "0123456789012345678901234567890123456789", "1234567890");
	fst_check_string_equals("AWS AKIAIOSFODNN7EXAMPLE:YJkomOaqUJlvEluDq4fpusID38Y=", authentication_header);
	switch_safe_free(authentication_header);

	authentication_header = aws_s3_authentication_create("GET", "https://vault.example.com/awesome.mp3", "example.com", "audio/mpeg", "", "AKIAIOSFODNN7EXAMPLE", "0123456789012345678901234567890123456789", "1234567890");
	fst_check_string_equals("AWS AKIAIOSFODNN7EXAMPLE:YJkomOaqUJlvEluDq4fpusID38Y=", authentication_header);
	switch_safe_free(authentication_header);
}
FST_TEST_END()

FST_TEST_BEGIN(test_presigned_url)
{
	char *presigned_url = aws_s3_presigned_url_create("GET", "https://vault.s3.amazonaws.com/awesome.mp3", NULL, "audio/mpeg", "", "AKIAIOSFODNN7EXAMPLE", "0123456789012345678901234567890123456789", "1234567890");
	fst_check_string_equals("https://vault.s3.amazonaws.com/awesome.mp3?Signature=YJkomOaqUJlvEluDq4fpusID38Y%3D&Expires=1234567890&AWSAccessKeyId=AKIAIOSFODNN7EXAMPLE", presigned_url);
	switch_safe_free(presigned_url);

	presigned_url = aws_s3_presigned_url_create("GET", "https://vault.s3.amazonaws.com/awesome.mp3", "s3.amazonaws.com", "audio/mpeg", "", "AKIAIOSFODNN7EXAMPLE", "0123456789012345678901234567890123456789", "1234567890");
	fst_check_string_equals("https://vault.s3.amazonaws.com/awesome.mp3?Signature=YJkomOaqUJlvEluDq4fpusID38Y%3D&Expires=1234567890&AWSAccessKeyId=AKIAIOSFODNN7EXAMPLE", presigned_url);
	switch_safe_free(presigned_url);

	presigned_url = aws_s3_presigned_url_create("GET", "https://vault.example.com/awesome.mp3", "example.com", "audio/mpeg", "", "AKIAIOSFODNN7EXAMPLE", "0123456789012345678901234567890123456789", "1234567890");
	fst_check_string_equals("https://vault.example.com/awesome.mp3?Signature=YJkomOaqUJlvEluDq4fpusID38Y%3D&Expires=1234567890&AWSAccessKeyId=AKIAIOSFODNN7EXAMPLE", presigned_url);
	switch_safe_free(presigned_url);
}
FST_TEST_END()

}
FST_SUITE_END()

}
FST_END()

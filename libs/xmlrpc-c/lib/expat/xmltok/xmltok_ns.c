const ENCODING *
NS(xmlrpc_XmlGetUtf8InternalEncoding)(void) {

    return &ns(internal_utf8_encoding).enc;
}



const ENCODING *
NS(xmlrpc_XmlGetUtf16InternalEncoding)(void) {

#if XML_BYTE_ORDER == 12
    return &ns(internal_little2_encoding).enc;
#elif XML_BYTE_ORDER == 21
    return &ns(internal_big2_encoding).enc;
#else
    const short n = 1;
    return *(const char *)&n ?
        &ns(internal_little2_encoding).enc :
        &ns(internal_big2_encoding).enc;
#endif
}

static
const ENCODING *NS(encodings)[] = {
  &ns(latin1_encoding).enc,
  &ns(ascii_encoding).enc,
  &ns(utf8_encoding).enc,
  &ns(big2_encoding).enc,
  &ns(big2_encoding).enc,
  &ns(little2_encoding).enc,
  &ns(utf8_encoding).enc /* NO_ENC */
};

static
int NS(initScanProlog)(const ENCODING *enc, const char *ptr, const char *end,
		       const char **nextTokPtr)
{
  return initScan(NS(encodings), (const INIT_ENCODING *)enc, XML_PROLOG_STATE, ptr, end, nextTokPtr);
}

static
int NS(initScanContent)(const ENCODING *enc, const char *ptr, const char *end,
		       const char **nextTokPtr)
{
  return initScan(NS(encodings), (const INIT_ENCODING *)enc, XML_CONTENT_STATE, ptr, end, nextTokPtr);
}



int
NS(xmlrpc_XmlInitEncoding)(INIT_ENCODING *   const p,
                           const ENCODING ** const encPtr,
                           const char *      const name) {

    int i = getEncodingIndex(name);
    if (i == UNKNOWN_ENC)
        return 0;
    SET_INIT_ENC_INDEX(p, i);
    p->initEnc.scanners[XML_PROLOG_STATE] = NS(initScanProlog);
    p->initEnc.scanners[XML_CONTENT_STATE] = NS(initScanContent);
    p->initEnc.updatePosition = initUpdatePosition;
    p->encPtr = encPtr;
    *encPtr = &(p->initEnc);
    return 1;
}



static
const ENCODING *NS(findEncoding)(const ENCODING *enc, const char *ptr, const char *end)
{
#define ENCODING_MAX 128
  char buf[ENCODING_MAX];
  char *p = buf;
  int i;
  XmlUtf8Convert(enc, &ptr, end, &p, p + ENCODING_MAX - 1);
  if (ptr != end)
    return 0;
  *p = 0;
  if (streqci(buf, KW_UTF_16) && enc->minBytesPerChar == 2)
    return enc;
  i = getEncodingIndex(buf);
  if (i == UNKNOWN_ENC)
    return 0;
  return NS(encodings)[i];
}



int
NS(xmlrpc_XmlParseXmlDecl)(int               const isGeneralTextEntity,
                           const ENCODING *  const enc,
                           const char *      const ptr,
                           const char *      const end,
                           const char **     const badPtr,
                           const char **     const versionPtr,
                           const char **     const encodingName,
                           const ENCODING ** const encoding,
                           int *             const standalone) {

  return doParseXmlDecl(NS(findEncoding),
                        isGeneralTextEntity,
                        enc,
                        ptr,
                        end,
                        badPtr,
                        versionPtr,
                        encodingName,
                        encoding,
                        standalone);
}

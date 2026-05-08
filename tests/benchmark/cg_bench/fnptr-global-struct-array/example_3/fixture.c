/* CG-Bench fixture: fnptr-global-struct-array/example_3 */
/* fnptr: mappings[i].writefunc, targets: writeLong, writeOffset, writeString, writeTime */

void ourWriteOutJSON(FILE *stream, const struct writeoutvar mappings[],
                     struct per_transfer *per, CURLcode per_result)
{
  int i;

  fputs("{", stream);

  for(i = 0; mappings[i].name != NULL; i++) {
    if(mappings[i].writefunc &&
       mappings[i].writefunc(stream, &mappings[i], per, per_result, true))
      fputs(",", stream);
  }

  /* The variables are sorted in alphabetical order but as a special case
     curl_version (which is not actually a --write-out variable) is last. */
  fprintf(stream, "\"curl_version\":");
  jsonWriteString(stream, curl_version(), FALSE);
  fprintf(stream, "}");
}

void ourWriteOut(struct OperationConfig *config, struct per_transfer *per,
                 CURLcode per_result)
{
  FILE *stream = stdout;
  const char *writeinfo = config->writeout;
  const char *ptr = writeinfo;
  bool done = FALSE;
  struct curl_certinfo *certinfo;
  CURLcode res = curl_easy_getinfo(per->curl, CURLINFO_CERTINFO, &certinfo);
  bool fclose_stream = FALSE;
  ...
       case VAR_JSON:
         ourWriteOutJSON(stream, variables, per, per_result);
         break;
  ...
}

static const struct writeoutvar variables[] = {
{"certs", VAR_CERT, CURLINFO_NONE, writeString},
 ...
  {"onerror", VAR_ONERROR, CURLINFO_NONE, NULL},
  {"proxy_ssl_verify_result", VAR_PROXY_SSL_VERIFY_RESULT,
   CURLINFO_PROXY_SSL_VERIFYRESULT, writeLong},
  ...
  {"scheme", VAR_SCHEME, CURLINFO_SCHEME, writeString},
  {"size_download", VAR_SIZE_DOWNLOAD, CURLINFO_SIZE_DOWNLOAD_T, writeOffset},
  ...
  {"time_connect", VAR_CONNECT_TIME, CURLINFO_CONNECT_TIME_T, writeTime},
  {"time_namelookup", VAR_NAMELOOKUP_TIME, CURLINFO_NAMELOOKUP_TIME_T,
   writeTime},
  {"time_total", VAR_TOTAL_TIME, CURLINFO_TOTAL_TIME_T, writeTime},
  {"url", VAR_INPUT_URL, CURLINFO_NONE, writeString},
  {"url.scheme", VAR_INPUT_URLSCHEME, CURLINFO_NONE, writeString},
  {"url.user", VAR_INPUT_URLUSER, CURLINFO_NONE, writeString},
  {"url.password", VAR_INPUT_URLPASSWORD, CURLINFO_NONE, writeString},
  ...
};


/* Stub implementation for writeLong */
void writeLong(void) {}



/* Stub implementation for writeOffset */
void writeOffset(void) {}



/* Stub implementation for writeString */
void writeString(void) {}



/* Stub implementation for writeTime */
void writeTime(void) {}

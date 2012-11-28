
#ifndef SHELLFM_REST
#define SHELLFM_REST

#define API_KEY "cc299dde20a9ae5dbb63118c65cbcc85"
#define API_SECRET "8aa5ec0f4dc57a9a6bd1732fdc9421a0"
#define REST_API_BASE "http://ws.audioscrobbler.com/2.0/"

#include "hash.h"
#include "json.h"

extern char * rest(const char *, struct hash *);
extern void create_session();
extern const char * error_message(const char *);
extern void json_hash(json_value *, struct hash *, const char *);
extern json_value * json_query(json_value *, ...);
extern const char * json_value_string(json_value *);

#endif

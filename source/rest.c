
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdarg.h>

#include "rest.h"
#include "http.h"
#include "util.h"
#include "hash.h"
#include "strary.h"
#include "json.h"
#include "settings.h"


static int cmpstringp(const void *, const void *);
static int restore_key(const char *, char *);
const char const * build_signature(struct hash *);
void create_token();


char * rest(const char * method, struct hash * p) {
	char * response;

	if(haskey(& rc, "session")) {
		set(p, "sk", value(& rc, "session"));
	}

	set(p, "format", "json");
	set(p, "method", method);
	set(p, "api_key", API_KEY);
	set(p, "api_sig", build_signature(p));

	debug("rest call signuture: <%s>\n", value(p, "api_sig"));

	response = fetch(REST_API_BASE, NULL, hash_query(p), NULL);

	debug("response:\n%s\n", response);

	return response;
}


void create_session(void) {
	char token[32 + 1] = { 0, }, session_key[32 + 1] = { 0, }, * response;
	struct hash h = { 0, NULL };
	json_value * json;

	if(!restore_key("session", session_key)) {
		if(!restore_key("token", token)) {
			create_token();
		}

		debug("token=<%s>\n", token);

		set(& h, "token", token);

		response = rest("auth.getSession", & h);
		if(!response)
			return;
		json = json_parse(response);

		free(response);

		empty(& h);

		json_hash(json, & h, NULL);
		json_value_free(json);

		if(haskey(& h, "session.key")) {
			strcpy(session_key, value(& h, "session.key"));
			set(& rc, "username", value(& h, "session.name"));
			spit(rcpath("session"), session_key);
			empty(& h);
		}
		else {
			if(haskey(& h, "message")) {
				fprintf(stderr, "%s.\n", value(& h, "message"));
			}

			empty(& h);

			fputs("Failed to create session. Please refresh authorization.\n", stderr);
			create_token();
		}
	}

	debug("session key = %s\n", session_key);
	set(& rc, "session", session_key);
}


void create_token(void) {
	json_value * json;
	struct hash p = { 0, NULL };
	char * json_plain = rest("auth.getToken", & p);

	debug("json:\n%s\n", json_plain);
	if (!json_plain)
		return;

	assert(json_plain != NULL);

	json = json_parse(json_plain);

	empty(& p);
	json_hash(json, & p, NULL);

	debug("token = <%s>\n", value(& p, "token"));

	set(& p, "api_key", API_KEY);

	spit(rcpath("token"), value(& p, "token"));

	fprintf(stderr, "Please open http://www.last.fm/api/auth/?%s in your browser to authorize shell-fm.\n", hash_query(& p));

	empty(& p);
	free(json_plain);
	json_value_free(json);

	exit(EXIT_SUCCESS);
}


const char const * build_signature(struct hash * p) {
	static char signature[32 + 1] = { 0, };
	unsigned n, length = 0;
	char ** const names = malloc(sizeof(char *) * p->size);
	char * plain;

	for(n = 0; n < p->size; ++n) {
		debug("build_signature <%s> => <%s>\n", p->content[n].key, p->content[n].value);
		names[n] = p->content[n].key;
		length += strlen(p->content[n].key) + strlen(p->content[n].value);
	}

	qsort(names, p->size, sizeof(* names), cmpstringp);

	plain = (char *) malloc(length + sizeof(API_SECRET) + 1);

	memset(plain, 0, length + sizeof(API_SECRET) + 1);

	for(n = 0, length = 0; n < p->size; ++n) {
		debug("sorted %i: %s => %s\n", n, names[n], value(p, names[n]));

		if(strcmp("format", names[n]) == 0 || strcmp("callback", names[n]) == 0) {
			debug("not including %s in signature\n", names[n]);
			continue;
		}

		strcat(plain, names[n]);
		strcat(plain, value(p, names[n]));
	}

	strcat(plain, API_SECRET);

	debug("plain signature: <%s>\n", plain);

	strncpy(signature, plain_md5(plain), 32);

	free(names);
	free(plain);

	return signature;
}


static int cmpstringp(const void * p1, const void * p2) {
	return strcmp(* (char * const *) p1, * (char * const *) p2);
}

static int restore_key(const char * type, char * b) {
	FILE * fd = fopen(rcpath(type), "r");

	if(fd != NULL) {
		int result = fscanf(fd, "%32s", b);

		fclose(fd);

		return result;
	}

	else {
		return 0;
	}
}


const char * error_message(const char * plain_json) {
	struct hash h = { 0, NULL };
	static char message[512] = { 0, };
	json_value * json = json_parse(plain_json);

	json_hash(json, & h, NULL);
	json_value_free(json);

	if(haskey(& h, "error")) {
		snprintf(message, sizeof(message), "%s", value(& h, "message"));
		empty(& h);
		return message;
	}

	empty(& h);

	return NULL;
}

json_value * json_query(json_value * root, ...) {
   va_list argv;
   const char * p;

   va_start(argv, root);

   while(root != NULL && (p = va_arg(argv, const char *))) {
      unsigned n, found = 0;

      switch(root->type) {
         case json_object:
            for(n = 0; n < root->u.object.length; ++n) {
               if(strcmp(p, root->u.object.values[n].name) == 0) {
                  root = root->u.object.values[n].value;
                  found = 1;
                  break;
               }
            }

            break;

         case json_array:
            n = atoi(p);

            if(n < root->u.array.length) {
               root = root->u.array.values[n];
               found = 1;
            }

            break;

         default:
            root = NULL;
            break;
      }

      if(!found) {
         root = NULL;
      }
   }

   return root;
}


void json_hash(json_value * root, struct hash * h, const char * prefix) {
   char key[512] = { 0, };
   unsigned n;
   const char * string_value;

   assert(root->type == json_object || root->type == json_array);

   if(root->type == json_object) {
      for(n = 0; n < root->u.object.length; ++n) {
         json_value * value = root->u.object.values[n].value;
         char * name = root->u.object.values[n].name;

         if(prefix != NULL && strlen(prefix) > 0)
            snprintf(key, sizeof(key), "%s.%s", prefix, name);
         else
            snprintf(key, sizeof(key), "%s", name);

         string_value = json_value_string(value);

         if(string_value != NULL) {
            set(h, key, string_value);
         }
         else if(value->type == json_object || value->type == json_array) {
            json_hash(value, h, key);
         }
      }
   }
   else if(root->type == json_array) {
      for(n = 0; n < root->u.array.length; ++n) {
         json_value * value = root->u.array.values[n];

         if(prefix != NULL && strlen(prefix) > 0)
            snprintf(key, sizeof(key), "%s.%d", prefix, n);
         else
            snprintf(key, sizeof(key), "%d", n);

         string_value = json_value_string(value);

         if(string_value != NULL) {
            set(h, key, string_value);
         }
         else if(value->type == json_object || value->type == json_array) {
            json_hash(value, h, key);
         }
      }
   }
}


const char * json_value_string(json_value * value) {
   static char buf[512];

   switch(value->type) {
      case json_string:
         return value->u.string.ptr;

      case json_boolean:
         return value->u.boolean ? "true" : "false";

      case json_integer:
         snprintf(buf, sizeof(buf), "%li", value->u.integer);
         return buf;

      case json_double:
         snprintf(buf, sizeof(buf), "%f", value->u.dbl);
         return buf;

      default:
         return NULL;
   }
}

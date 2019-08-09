/*
Manually modify MONGOC_TOPOLOGY_MIN_RESCAN_SRV_INTERVAL_MS to be 1 second.
And ensure the TTLs are also 1 second.
 */

#include <mongoc/mongoc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mongoc/mongoc-client-private.h"
#include "mongoc/mongoc-topology-private.h"
#include "mongoc/mongoc-uri-private.h"

/* A hack to reach inside URI struct. */
struct _mongoc_uri_t {
   char *str;
   bool is_srv;
   char srv[BSON_HOST_NAME_MAX + 1];
   mongoc_host_list_t *hosts;
   char *username;
   char *password;
   char *database;
   bson_t raw;     /* Unparsed options, see mongoc_uri_parse_options */
   bson_t options; /* Type-coerced and canonicalized options */
   bson_t credentials;
   bson_t compressors;
   mongoc_read_prefs_t *read_prefs;
   mongoc_read_concern_t *read_concern;
   mongoc_write_concern_t *write_concern;
};

void
_mongoc_host_list_dump (const mongoc_host_list_t *host) {
   printf("host_list:\n");
   while (host != NULL) {
      printf("-%s\n", host->host_and_port);
      host = host->next;
   }
}

int
main (int argc, char *argv[])
{
   mongoc_client_t *client;
   mongoc_collection_t *collection;
   mongoc_cursor_t *cursor;
   bson_error_t error;
   const bson_t *doc;
   const char *collection_name = "test";
   bson_t query;
   char *str;
   const char *uri_string = "mongodb://127.0.0.1/?appname=client-example";
   mongoc_uri_t *uri;

   mongoc_init ();
   if (argc > 1) {
      uri_string = argv[1];
   }

   if (argc > 2) {
      collection_name = argv[2];
   }

   uri = mongoc_uri_new_with_error (uri_string, &error);
   if (!uri) {
      fprintf (stderr,
               "failed to parse URI: %s\n"
               "error message:       %s\n",
               uri_string,
               error.message);
      return EXIT_FAILURE;
   }

   client = mongoc_client_new_from_uri (uri);
   if (!client) {
      return EXIT_FAILURE;
   }

   mongoc_client_set_error_api (client, 2);

   bson_init (&query);

#if 0
   bson_append_utf8 (&query, "hello", -1, "world", -1);
#endif

   int secs = 0;
   while (true) {
      collection = mongoc_client_get_collection (client, "test", collection_name);
      cursor = mongoc_collection_find_with_opts (
         collection,
         &query,
         NULL,  /* additional options */
         NULL); /* read prefs, NULL for default */

      while (mongoc_cursor_next (cursor, &doc)) {
         str = bson_as_canonical_extended_json (doc, NULL);
         fprintf (stdout, "%s\n", str);
         bson_free (str);
      }

      if (mongoc_cursor_error (cursor, &error)) {
         fprintf (stderr, "Cursor Failure: %s\n", error.message);
         return EXIT_FAILURE;
      }
      printf("it has been %d seconds\n", secs);
      secs++;
      sleep(1);

      _mongoc_host_list_dump (client->topology->uri->hosts);

      mongoc_cursor_destroy (cursor);
      mongoc_collection_destroy (collection);
      if (secs > 60) break;
   }

   bson_destroy (&query);
   mongoc_uri_destroy (uri);
   mongoc_client_destroy (client);
   mongoc_cleanup ();

   return EXIT_SUCCESS;
}

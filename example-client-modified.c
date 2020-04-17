/*
Manually modify MONGOC_TOPOLOGY_MIN_RESCAN_SRV_INTERVAL_MS to be 1 second.
And ensure the TTLs are also 1 second.
Set heartbeatFrequencyMS in the URI to 1 second.
 */

#include <mongoc/mongoc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define MONGOC_INSIDE
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
_host_list_dump (const mongoc_host_list_t *host)
{
   MONGOC_DEBUG ("host_list:");
   while (host != NULL) {
      printf ("- %s\n", host->host_and_port);
      host = host->next;
   }
}


static const char *
_sd_type (mongoc_server_description_t *sd)
{
   switch (sd->type) {
   case MONGOC_SERVER_RS_PRIMARY:
      return "MONGOC_SERVER_RS_PRIMARY";
   case MONGOC_SERVER_RS_SECONDARY:
      return "MONGOC_SERVER_RS_SECONDARY";
   case MONGOC_SERVER_STANDALONE:
      return "MONGOC_SERVER_STANDALONE";
   case MONGOC_SERVER_MONGOS:
      return "MONGOC_SERVER_MONGOS";
   case MONGOC_SERVER_POSSIBLE_PRIMARY:
      return "MONGOC_SERVER_POSSIBLE_PRIMARY";
   case MONGOC_SERVER_RS_ARBITER:
      return "MONGOC_SERVER_RS_ARBITER";
   case MONGOC_SERVER_RS_OTHER:
      return "MONGOC_SERVER_RS_OTHER";
   case MONGOC_SERVER_RS_GHOST:
      return "MONGOC_SERVER_RS_GHOST";
   case MONGOC_SERVER_UNKNOWN:
      return "MONGOC_SERVER_UNKNOWN";
   case MONGOC_SERVER_DESCRIPTION_TYPES:
   default:
      return "??";
   }
   return "??";
}

static void
_topology_description_dump (mongoc_topology_description_t *td)
{
   int i;
   mongoc_set_t *set;
   mongoc_server_description_t *sd;

   set = td->servers;

   MONGOC_DEBUG ("Topology description dump (%s):",
                 mongoc_topology_description_type (td));

   for (i = 0; i < set->items_len; ++i) {
      mongoc_host_list_t *host;
      sd = (mongoc_server_description_t *) mongoc_set_get_item (set, (int) i);

      host = mongoc_server_description_host (sd);
      printf ("- %s, %s\n", host->host_and_port, _sd_type (sd));
   }
}

int
main (int argc, char *argv[])
{
   mongoc_client_pool_t *pool;
   mongoc_client_t *client;
   bson_error_t error;
   bson_t *cmd;
   char *str;
   char *uri_string;
   mongoc_uri_t *uri;
   bool pooled;

   mongoc_init ();
   if (argc != 3) {
      MONGOC_ERROR ("Usage: ./example-client <URI> <\"POOLED\"|\"SINGLE\">\n");
      return EXIT_FAILURE;
   }

   uri_string = argv[1];
   pooled = (strcasecmp (argv[2], "POOLED") == 0);

   uri = mongoc_uri_new_with_error (uri_string, &error);
   if (!uri) {
      MONGOC_ERROR ("failed to parse URI: %s, error message: %s\n",
                    uri_string,
                    error.message);
      return EXIT_FAILURE;
   }

   if (pooled) {
      pool = mongoc_client_pool_new (uri);
      BSON_ASSERT (pool);
      mongoc_client_pool_set_error_api (pool, 2);
      client = mongoc_client_pool_pop (pool);
   } else {
      client = mongoc_client_new_from_uri (uri);
      BSON_ASSERT (client);
      mongoc_client_set_error_api (client, 2);
   }

   int secs = 0;
   cmd = BCON_NEW ("ping", BCON_INT32 (1));
   while (true) {
      bson_t reply;
      bool ret;

      ret = mongoc_client_command_simple (
         client, "admin", cmd, NULL, &reply, &error);
      str = bson_as_json (&reply, NULL);
      MONGOC_DEBUG ("reply = %s", str);
      bson_free (str);
      bson_destroy (&reply);

      if (!ret) {
         MONGOC_ERROR ("ping failure: %s\n", error.message);
         return EXIT_FAILURE;
      }
      printf ("it has been %d seconds\n\n", secs);
      secs++;
      sleep (1);

      bson_mutex_lock (&client->topology->mutex);
      _host_list_dump (client->topology->uri->hosts);
      _topology_description_dump (&client->topology->description);
      bson_mutex_unlock (&client->topology->mutex);

      if (secs > 60)
         break;
   }

   mongoc_uri_destroy (uri);
   mongoc_client_destroy (client);
   mongoc_cleanup ();

   return EXIT_SUCCESS;
}

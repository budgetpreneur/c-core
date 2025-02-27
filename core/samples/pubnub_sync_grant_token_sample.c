/* -*- c-file-style:"stroustrup"; indent-tabs-mode: nil -*- */
#include "pubnub_sync.h"

#include "core/pubnub_helper.h"
#include "core/pubnub_timers.h"
#include "core/pubnub_crypto.h"
#include "core/pubnub_grant_token_api.h"
#include "core/pubnub_json_parse.h"

#include <stdio.h>
#include <time.h>

static void generate_uuid(pubnub_t* pbp)
{
    char const*                      uuid_default = "zeka-peka-iz-jendeka";
    struct Pubnub_UUID               uuid;
    static struct Pubnub_UUID_String str_uuid;

    if (0 != pubnub_generate_uuid_v4_random(&uuid)) {
        pubnub_set_uuid(pbp, uuid_default);
    }
    else {
        str_uuid = pubnub_uuid_to_string(&uuid);
        pubnub_set_uuid(pbp, str_uuid.uuid);
        printf("Generated UUID: %s\n", str_uuid.uuid);
    }
}

static void sync_sample_free(pubnub_t* p)
{
    if (PN_CANCEL_STARTED == pubnub_cancel(p)) {
        enum pubnub_res pnru = pubnub_await(p);
        if (pnru != PNR_OK) {
            printf("Awaiting cancel failed: %d('%s')\n",
                   pnru,
                   pubnub_res_2_string(pnru));
        }
    }
    if (pubnub_free(p) != 0) {
        printf("Failed to free the Pubnub context\n");
    }
}


static int do_time(pubnub_t* pbp)
{
    enum pubnub_res res;

    puts("-----------------------");
    puts("Getting time...");
    puts("-----------------------");
    res = pubnub_time(pbp);
    if (PNR_STARTED == res) {
        res = pubnub_await(pbp);
    }
    if (PNR_OK == res) {
        printf("Gotten time: %s; last time token=%s\n",
               pubnub_get(pbp),
               pubnub_last_time_token(pbp));
    }
    else {
        printf("Getting time failed with code: %d('%s')\n",
               res,
               pubnub_res_2_string(res));
    }

    return 0;
}

int main()
{
    time_t          t0;
    char const*     msg;
    enum pubnub_res res;
    char const*     chan = "hello_world";
    pubnub_t*       gtp  = pubnub_alloc();
    pubnub_t*       pbp  = pubnub_alloc();

    if (NULL == pbp || NULL == gtp) {
        printf("Failed to allocate Pubnub context!\n");
        return -1;
    }

    char* my_env_publish_key = getenv("PUBNUB_PUBLISH_KEY");
    char* my_env_subscribe_key = getenv("PUBNUB_SUBSCRIBE_KEY");
    char* my_env_secret_key = getenv("PUBNUB_SECRET_KEY");

    if (NULL == my_env_publish_key) { my_env_publish_key = "demo"; }
    if (NULL == my_env_subscribe_key) { my_env_subscribe_key = "demo"; }
    if (NULL == my_env_secret_key) { my_env_secret_key = "demo"; }

    printf("%s\n%s\n%s\n",my_env_publish_key,my_env_subscribe_key,my_env_secret_key);

    pubnub_init(gtp, my_env_publish_key, my_env_subscribe_key);
    pubnub_set_secret_key(gtp, my_env_secret_key);

    pubnub_set_transaction_timeout(gtp, PUBNUB_DEFAULT_SUBSCRIBE_TIMEOUT);

    /* Leave this commented out to use the default - which is
       blocking I/O on most platforms. Uncomment to use non-
       blocking I/O.
    */
    pubnub_set_non_blocking_io(gtp);

    generate_uuid(gtp);

    puts("Grant Token...");
    time(&t0);
    struct pam_permission h_perm = {.read=true, .write=true };
    int perm_hello_world = pubnub_get_grant_bit_mask_value(h_perm);
    struct pam_permission cg_perm = {.read=true, .write=true, .manage=true};
    int perm_channel_group = pubnub_get_grant_bit_mask_value(cg_perm);
    int ttl_minutes = 60;
    char perm_obj[2000];
    char* authorized_uuid = "my_authorized_uuid";
    sprintf(perm_obj,"{\"ttl\":%d, \"uuid\":\"%s\", \"permissions\":{\"resources\":{\"channels\":{ \"mych\":31, \"hello_world\":%d }, \"groups\":{ \"mycg\":31, \"channel-group\":%d }, \"users\":{ \"myuser\":31 }, \"spaces\":{ \"myspc\":31 }}, \"patterns\":{\"channels\":{ }, \"groups\":{ }, \"users\":{ \"^$\":1 }, \"spaces\":{ \"^$\":1 }},\"meta\":{ }}}", ttl_minutes, authorized_uuid, perm_hello_world, perm_channel_group);
    res = pubnub_grant_token(gtp, perm_obj);
    pubnub_chamebl_t grant_token_resp;
    if (PNR_STARTED == res) {
        res = pubnub_await(gtp);
        if (PNR_OK == res) {
            //const char* grant_token_resp_json = pubnub_get(gtp);
            //printf("pubnub_grant_token() JSON Response from Pubnub: %s\n", grant_token_resp_json);
            // OR below code
            grant_token_resp = pubnub_get_grant_token(gtp);
            printf("pubnub_grant_token() Response from Pubnub: %s\n", grant_token_resp.ptr);

            char* cbor_data = pubnub_parse_token(gtp, grant_token_resp.ptr);
            printf("pubnub_parse_token() = %s\n", cbor_data);
        }
        else{
            printf("pubnub_grant_token() failed with code: %d('%s')\n",
                res,
                pubnub_res_2_string(res));
        }
    }
    else{
        printf("pubnub_grant_token() failed with code: %d('%s')\n",
               res,
               pubnub_res_2_string(res));
    }

    char* auth_token = grant_token_resp.ptr;
    pubnub_init(pbp, my_env_publish_key, my_env_subscribe_key);
    pubnub_set_auth_token(pbp, auth_token);

    pubnub_set_transaction_timeout(gtp, PUBNUB_DEFAULT_SUBSCRIBE_TIMEOUT);

    /* Leave this commented out to use the default - which is
       blocking I/O on most platforms. Uncomment to use non-
       blocking I/O.
    */
    pubnub_set_non_blocking_io(pbp);

    generate_uuid(pbp);

    puts("Publishing...");
    time(&t0);
    res = pubnub_publish(pbp, chan, "\"Hello world from sync!\"");
    if (PNR_STARTED == res) {
        res = pubnub_await(pbp);
    }
    printf("Publish lasted %lf seconds.\n", difftime(time(NULL), t0));
    if (PNR_OK == res) {
        printf("Published! Response from Pubnub: %s\n",
               pubnub_last_publish_result(pbp));
    }
    else if (PNR_PUBLISH_FAILED == res) {
        printf("Published failed on Pubnub, description: %s\n",
               pubnub_last_publish_result(pbp));
    }
    else {
        printf("Publishing failed with code: %d('%s')\n",
               res,
               pubnub_res_2_string(res));
    }

    puts("Subscribing...");
    time(&t0);
    res = pubnub_subscribe(pbp, chan, NULL);
    if (PNR_STARTED == res) {
        res = pubnub_await(pbp);
    }
    printf("Subscribe/connect lasted %lf seconds.\n", difftime(time(NULL), t0));
    if (PNR_OK == res) {
        puts("Subscribed!");
    }
    else {
        printf("Subscribing failed with code: %d('%s')\n",
               res,
               pubnub_res_2_string(res));
    }

    time(&t0);
    res = pubnub_subscribe(pbp, chan, NULL);
    if (PNR_STARTED == res) {
        res = pubnub_await(pbp);
    }
    printf("Subscribe lasted %lf seconds.\n", difftime(time(NULL), t0));
    if (PNR_OK == res) {
        puts("Subscribed! Got messages:");
        for (;;) {
            msg = pubnub_get(pbp);
            if (NULL == msg) {
                break;
            }
            puts(msg);
        }
    }
    else {
        printf("Subscribing failed with code: %d('%s')\n",
               res,
               pubnub_res_2_string(res));
    }

    res = pubnub_heartbeat(pbp, chan, NULL);
    if (PNR_STARTED == res) {
        res = pubnub_await(pbp);
    }
    if (PNR_OK == res) {
        puts("Heartbeated! Got messages:");
        for (;;) {
            msg = pubnub_get(pbp);
            if (NULL == msg) {
                break;
            }
            puts(msg);
        }
    }
    else {
        printf("Heartbeating failed with code: %d('%s')\n",
               res,
               pubnub_res_2_string(res));
    }

    if (do_time(pbp) == -1) {
        return -1;
    }

    puts("Getting history with include_token...");
    res = pubnub_history(pbp, chan, 10, true);
    if (PNR_STARTED == res) {
        res = pubnub_await(pbp);
    }
    if (PNR_OK == res) {
        puts("Got history! Elements:");
        for (;;) {
            msg = pubnub_get(pbp);
            if (NULL == msg) {
                break;
            }
            puts(msg);
        }
    }
    else {
        printf("Getting history v2 with include_token failed with code: "
               "%d('%s')\n",
               res,
               pubnub_res_2_string(res));
    }

    puts("Getting here_now presence...");
    res = pubnub_here_now(pbp, chan, NULL);
    if (PNR_STARTED == res) {
        res = pubnub_await(pbp);
    }
    if (PNR_OK == res) {
        puts("Got here now presence!");
        for (;;) {
            msg = pubnub_get(pbp);
            if (NULL == msg) {
                break;
            }
            puts(msg);
        }
    }
    else {
        printf("Getting here-now presence failed with code: %d('%s')\n",
               res,
               pubnub_res_2_string(res));
    }

    /** Global here_now presence for "demo" subscribe key is _very_
        long, but we have it here to show that we can handle very long
        response if the PUBNUB_DYNAMIC_REPLY_BUFFER is "on".
    */
    if (PUBNUB_DYNAMIC_REPLY_BUFFER) {
        puts("Getting global here_now presence...");
        res = pubnub_global_here_now(pbp);
        if (PNR_STARTED == res) {
            res = pubnub_await(pbp);
        }
        if (PNR_OK == res) {
            puts("Got global here now presence!");
            for (;;) {
                msg = pubnub_get(pbp);
                if (NULL == msg) {
                    break;
                }
                puts(msg);
            }
        }
        else {
            printf(
                "Getting global here-now presence failed with code: %d('%s')\n",
                res,
                pubnub_res_2_string(res));
        }
    }

    puts("Getting where_now presence...");
    res = pubnub_where_now(pbp, pubnub_uuid_get(pbp));
    if (PNR_STARTED == res) {
        res = pubnub_await(pbp);
    }
    if (PNR_OK == res) {
        puts("Got where now presence!");
        for (;;) {
            msg = pubnub_get(pbp);
            if (NULL == msg) {
                break;
            }
            puts(msg);
        }
    }
    else {
        printf("Getting where-now presence failed with code: %d('%s')\n",
               res,
               pubnub_res_2_string(res));
    }

    puts("Setting state...");
    res = pubnub_set_state(pbp, chan, NULL, pubnub_uuid_get(pbp), "{\"x\":5}");
    if (PNR_STARTED == res) {
        res = pubnub_await(pbp);
    }
    if (PNR_OK == res) {
        puts("Got set state response!");
        for (;;) {
            msg = pubnub_get(pbp);
            if (NULL == msg) {
                break;
            }
            puts(msg);
        }
    }
    else {
        printf("Setting state failed with code: %d('%s')\n",
               res,
               pubnub_res_2_string(res));
    }

    puts("Getting state...");
    res = pubnub_state_get(pbp, chan, NULL, pubnub_uuid_get(pbp));
    if (PNR_STARTED == res) {
        res = pubnub_await(pbp);
    }
    if (PNR_OK == res) {
        puts("Got state!");
        for (;;) {
            msg = pubnub_get(pbp);
            if (NULL == msg) {
                break;
            }
            puts(msg);
        }
    }
    else {
        printf("Getting state failed with code: %d('%s')\n",
               res,
               pubnub_res_2_string(res));
    }

    puts("List channel group...");
    res = pubnub_list_channel_group(pbp, "channel-group");
    if (PNR_STARTED == res) {
        res = pubnub_await(pbp);
    }
    if (PNR_OK == res) {
        puts("Got channel group list!");
        for (;;) {
            msg = pubnub_get_channel(pbp);
            if (NULL == msg) {
                break;
            }
            puts(msg);
        }
    }
    else {
        printf("Getting channel group list failed with code: %d ('%s')\n",
               res,
               pubnub_res_2_string(res));
    }

    puts("Add channel to group");
    res = pubnub_add_channel_to_group(pbp, chan, "channel-group");
    if (PNR_STARTED == res) {
        res = pubnub_await(pbp);
    }
    if (PNR_OK == res) {
        puts("Got add channel to group response!");
        for (;;) {
            msg = pubnub_get_channel(pbp);
            if (NULL == msg) {
                break;
            }
            puts(msg);
        }
    }
    else {
        printf("Adding channel to group failed with code: %d('%s')\n",
               res,
               pubnub_res_2_string(res));
    }

    puts("Remove channel from group");
    res = pubnub_remove_channel_from_group(pbp, chan, "channel-group");
    if (PNR_STARTED == res) {
        res = pubnub_await(pbp);
    }
    if (PNR_OK == res) {
        puts("Got remove channel from group response!");
        for (;;) {
            msg = pubnub_get_channel(pbp);
            if (NULL == msg) {
                break;
            }
            puts(msg);
        }
    }
    else {
        printf("Removing channel from group failed with code: %d('%s')\n",
               res,
               pubnub_res_2_string(res));
    }

    puts("Remove channel group");
    res = pubnub_remove_channel_group(pbp, "channel-group");
    if (PNR_STARTED == res) {
        res = pubnub_await(pbp);
    }
    if (PNR_OK == res) {
        puts("Got remove channel group response!");
        for (;;) {
            msg = pubnub_get_channel(pbp);
            if (NULL == msg) {
                break;
            }
            puts(msg);
        }
    }
    else {
        printf("Removing channel group failed with code: %d('%s')\n",
               res,
               pubnub_res_2_string(res));
    }


    /* We're done, but, keep-alive might be on, so we need to cancel,
     * then free */
    sync_sample_free(pbp);
    sync_sample_free(gtp);

    puts("Pubnub sync grant token demo over.");

    return 0;
}

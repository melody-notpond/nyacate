#include <stdio.h>

#include <notcurses/notcurses.h>
#include <strophe.h>

void connection_handler(xmpp_conn_t* connection, xmpp_conn_event_t status, int error, xmpp_stream_error_t* stream_error, void* userdata) {
    xmpp_ctx_t *context = (xmpp_ctx_t*)userdata;
    (void) error;
    (void) stream_error;

    if (status == XMPP_CONN_CONNECT) {
        xmpp_stanza_t* presence = xmpp_presence_new(context);
        xmpp_send(connection, presence);
        xmpp_stanza_release(presence);
    } else xmpp_stop(context);
}

int main() {
    struct notcurses* nc;
    nc = notcurses_init(&(notcurses_options) {}, stdout);
    if (nc == NULL)
        return 1;
    notcurses_enter_alternate_screen(nc);
    //notcurses_mice_enable(nc, NCMICE_ALL_EVENTS);

    struct ncplane* stdplane = notcurses_stdplane(nc);
    struct ncplane* plane = ncplane_create(stdplane, &(struct ncplane_options) {
            .x = 0,
            .y = 0,
            .rows = 3,
            .cols = 50,
    });
    struct ncreader* reader = ncreader_create(plane, &(ncreader_options) {
        .flags = NCREADER_OPTION_CURSOR | NCREADER_OPTION_HORSCROLL,
    });
    ncreader_clear(reader);
    notcurses_render(nc);

    xmpp_initialize();
    xmpp_ctx_t* context = xmpp_ctx_new(NULL, NULL);
    xmpp_conn_t* connection = xmpp_conn_new(context);

    FILE* credentials = fopen(".credentials", "r");
    char username[50];
    size_t i;
    for (i = 0; i < sizeof(username) - 1; i++) {
        username[i] = fgetc(credentials);
        if (username[i] == '\n') {
            username[i] = '\0';
            break;
        }
    }
    username[sizeof(username) - 1] = '\0';

    char password[50];
    for (i = 0; i < sizeof(password) - 1; i++) {
        password[i] = fgetc(credentials);
        if (password[i] == '\n') {
            password[i] = '\0';
            break;
        }
    }
    password[sizeof(password) - 1] = '\0';
    fclose(credentials);

    xmpp_conn_set_jid(connection, username);
    xmpp_conn_set_pass(connection, password);
    if (xmpp_connect_client(connection, NULL, 0, connection_handler, context) != XMPP_EOK)
        goto cleanup;

    while (true) {
        xmpp_run_once(context, 10);
        struct ncinput event;
        if (!notcurses_get_nblock(nc, &event))
            continue;
        if (event.id == NCKEY_ENTER) {
            char* contents = ncreader_contents(reader);
            ncreader_clear(reader);
            if (!strcmp(contents, "/quit")) {
                free(contents);
                break;
            }

            xmpp_stanza_t* message = xmpp_message_new(context, "chat", "jenra@chat.lauwa.xyz", NULL);
            xmpp_message_set_body(message, contents);
            xmpp_send(connection, message);
            xmpp_stanza_release(message);
            free(contents);
        } else {
            ncreader_offer_input(reader, &event);
        }
        notcurses_render(nc);
    }

cleanup:
    xmpp_conn_release(connection);
    xmpp_ctx_free(context);
    xmpp_shutdown();

    ncreader_destroy(reader, NULL);
    //notcurses_mice_disable(nc);
    notcurses_leave_alternate_screen(nc);
    notcurses_stop(nc);
}

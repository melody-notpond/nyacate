#include <stdio.h>

#include <notcurses/notcurses.h>
#include <strophe.h>

#include "vector.h"

struct UserData {
    xmpp_ctx_t* context;
    //Vector* messages;
    struct ncplane* plane;
};

int message_handler(xmpp_conn_t* connection, xmpp_stanza_t* stanza, void* userdata) {
    (void) connection;
    struct UserData* data = (struct UserData*) userdata;
    xmpp_ctx_t* context = data->context;
    struct ncplane* message = data->plane;

    xmpp_stanza_t* body = xmpp_stanza_get_child_by_name(stanza, "body");
    if (body == NULL)
        return 1;

    const char* type = xmpp_stanza_get_type(stanza);
    if (type != NULL && strcmp(type, "error") == 0)
        return 1;

    char* body_text = xmpp_stanza_get_text(body);
    ncplane_erase(message);
    ncplane_putstr(message, body_text);
    xmpp_free(context, body_text);

    return 0;
}

void connection_handler(xmpp_conn_t* connection, xmpp_conn_event_t status, int error, xmpp_stream_error_t* stream_error, void* userdata) {
    struct UserData* data = (struct UserData*) userdata;
    xmpp_ctx_t* context = data->context;
    (void) error;
    (void) stream_error;

    if (status == XMPP_CONN_CONNECT) {
        xmpp_handler_add(connection, message_handler, NULL, "message", NULL, userdata);
        xmpp_stanza_t* presence = xmpp_presence_new(context);
        xmpp_send(connection, presence);
        xmpp_stanza_release(presence);
    } else xmpp_stop(context);
}

int resize_input_box(struct ncplane* input) {
    const struct ncplane* parent = ncplane_parent_const(input);
    unsigned int y, x;
    ncplane_dim_yx(parent, &y, &x);
    return ncplane_resize(input, 0, 0, 0, 0, 0, y - 1, 1, x);
}

typedef struct {
    char* message;
    struct ncplane* plane;
} Message;

void destroy_message(Message* message) {
    free(message->message);
    //ncplane_destroy(message->plane);
}

int main() {
    struct notcurses* nc;
    nc = notcurses_init(&(notcurses_options) {}, stdout);
    if (nc == NULL)
        return 1;
    notcurses_enter_alternate_screen(nc);
    //notcurses_mice_enable(nc, NCMICE_ALL_EVENTS);

    struct ncplane* stdplane = notcurses_stdplane(nc);
    unsigned int y, x;
    ncplane_dim_yx(stdplane, &y, &x);
    struct ncplane* plane = ncplane_create(stdplane, &(struct ncplane_options) {
            .x = 0,
            .y = y - 1,
            .rows = 1,
            .cols = x,
            .resizecb = resize_input_box,
    });
    nccell cell = {};
    nccell_set_bg_rgb8(&cell, 0xff, 0xff, 0xff);
    ncplane_base(plane, &cell);
    struct ncreader* reader = ncreader_create(plane, &(ncreader_options) {
        .flags = NCREADER_OPTION_CURSOR | NCREADER_OPTION_HORSCROLL,
    });
    ncreader_clear(reader);

    struct ncplane* message = ncplane_create(stdplane, &(struct ncplane_options) {
        .x = 0,
        .y = 0,
        .rows = 1,
        .cols = 50,
    });

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
    struct UserData data = {
        .context = context,
        .plane = message,
    };
    if (xmpp_connect_client(connection, NULL, 0, connection_handler, &data) != XMPP_EOK)
        goto cleanup;

    //Vector* messages = create_vector(Message);

    while (true) {
        xmpp_run_once(context, 10);
        struct ncinput event;
        if (!notcurses_get_nblock(nc, &event)) {
            notcurses_render(nc);
            continue;
        }

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

    //destroy_vector(messages, destroy_message);
    ncplane_destroy(message);

cleanup:
    xmpp_conn_release(connection);
    xmpp_ctx_free(context);
    xmpp_shutdown();

    ncreader_destroy(reader, NULL);
    //notcurses_mice_disable(nc);
    notcurses_leave_alternate_screen(nc);
    notcurses_stop(nc);
}

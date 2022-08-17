#include <stdio.h>

#include <notcurses/notcurses.h>
#include <strophe.h>

#include "vector.h"

typedef struct {
    char* sender;
    char* message;

    struct nctablet* tablet;
    xmpp_ctx_t* context;
} Message;

void destroy_message(Message** message_ptr) {
    Message* message = *message_ptr;
    xmpp_free(message->context, message->sender);
    xmpp_free(message->context, message->message);
    free(message);
}

struct UserData {
    xmpp_ctx_t* context;
    Vector* messages;
    struct ncreel* messages_reel;
};

int draw_message(struct nctablet* tablet, bool drawfromtop) {
    (void) drawfromtop;

    struct ncplane* plane = nctablet_plane(tablet);
    Message* message = nctablet_userptr(tablet);
    struct ncplane* sender = ncplane_create(plane, &(struct ncplane_options) {
        .x = 0,
        .y = 0,
        .rows = 1,
        .cols = 20,
    });
    ncplane_putstr(sender, message->sender);
    unsigned int width = ncplane_dim_x(plane);
    struct ncplane* message_plane = ncplane_create(plane, &(struct ncplane_options) {
        .x = 2,
        .y = 1,
        .rows = 1,
        .cols = width - 2,
    });
    ncplane_putstr(message_plane, message->message);
    return 2;
}

int message_handler(xmpp_conn_t* connection, xmpp_stanza_t* stanza, void* userdata) {
    (void) connection;
    struct UserData* data = (struct UserData*) userdata;
    xmpp_ctx_t* context = data->context;
    Vector* messages = data->messages;
    struct ncreel* messages_reel = data->messages_reel;

    xmpp_stanza_t* body = xmpp_stanza_get_child_by_name(stanza, "body");
    if (body == NULL)
        return 1;

    const char* type = xmpp_stanza_get_type(stanza);
    if (type != NULL && strcmp(type, "error") == 0)
        return 1;

    char* body_text = xmpp_stanza_get_text(body);
    const char* from = xmpp_stanza_get_from(stanza);
    Message *message = malloc(sizeof(Message));
    *message = (Message) {
        .sender = (char*) from,
        .message = body_text,
        .context = context,
    };
    struct nctablet* tablet = ncreel_add(messages_reel, vector_len(messages) ? (*(Message**) vector_get(messages, vector_len(messages) - 1))->tablet : NULL, NULL, draw_message, message);
    ncreel_next(messages_reel);
    message->tablet = tablet;
    vector_push(messages, &message);
    return 1;
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
    struct ncreader* reader = ncreader_create(plane, &(ncreader_options) {
        .flags = NCREADER_OPTION_CURSOR | NCREADER_OPTION_HORSCROLL,
    });
    ncreader_clear(reader);
    plane = ncplane_create(stdplane, &(struct ncplane_options) {
            .x = 0,
            .y = 0,
            .rows = y - 1,
            .cols = x,
    });
    struct ncreel* messages_reel = ncreel_create(plane, &(ncreel_options) {
        .tabletmask = NCBOXMASK_TOP | NCBOXMASK_BOTTOM | NCBOXMASK_LEFT | NCBOXMASK_RIGHT,
    });

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

    Vector* messages = create_vector(Message*);
    struct UserData data = {
        .context = context,
        .messages = messages,
        .messages_reel = messages_reel,
    };

    if (xmpp_connect_client(connection, NULL, 0, connection_handler, &data) != XMPP_EOK)
        goto cleanup;

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

cleanup:
    ncreader_destroy(reader, NULL);
    ncreel_destroy(messages_reel);
    //notcurses_mice_disable(nc);
    notcurses_leave_alternate_screen(nc);
    notcurses_stop(nc);

    destroy_vector(messages, destroy_message);
    xmpp_conn_release(connection);
    xmpp_ctx_free(context);
    xmpp_shutdown();

}

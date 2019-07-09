#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <glib.h>
#include <glib-object.h>

#include <agent.h>

/********************************************************
 * STATIC VARIABLES
 ********************************************************/
static GMainContext * mainctx = NULL;
static GMainLoop * mainloop = NULL;
static GSource * sdp_file_check_timer = NULL;


/********************************************************
 * INTERNAL FUNCTIONS
 ********************************************************/
static void on_incomming_message_cb(NiceAgent * agent, guint stream_id, guint component_id,
                                    guint len, gchar * buf,
                                    gpointer user_data)
{
    printf("Data received from nice agent. Don't process it.\n");
    return;
}

static void on_state_changed_cb(NiceAgent * agent, guint stream_id, guint component_id,
                                guint state, gpointer user_data)
{
    switch (state)
    {
        case NICE_COMPONENT_STATE_DISCONNECTED :
            printf("component %d of stream %d has new state: NICE_COMPONENT_STATE_DISCONNECTED\n",component_id, stream_id);
        break;
        case NICE_COMPONENT_STATE_GATHERING    :
            printf("component %d of stream %d has new state: NICE_COMPONENT_STATE_GATHERING\n",component_id, stream_id);
        break;
        case NICE_COMPONENT_STATE_CONNECTING   :
            printf("component %d of stream %d has new state: NICE_COMPONENT_STATE_CONNECTING\n",component_id, stream_id);
        break;
        case NICE_COMPONENT_STATE_CONNECTED    :
            printf("component %d of stream %d has new state: NICE_COMPONENT_STATE_CONNECTED\n",component_id, stream_id);
            nice_agent_attach_recv(agent, stream_id, 1, mainctx,
                       on_incomming_message_cb, NULL);

        break;
        case NICE_COMPONENT_STATE_READY        :
            // TODO:
            //    Let's send some data!
            printf("component %d of stream %d has new state: NICE_COMPONENT_STATE_READY\n",component_id, stream_id);
        break;
        case NICE_COMPONENT_STATE_FAILED       :
            printf("component %d of stream %d has new state: NICE_COMPONENT_STATE_FAILED\n",component_id, stream_id);
        break;
        case NICE_COMPONENT_STATE_LAST         :
            printf("component %d of stream %d has new state: NICE_COMPONENT_STATE_LAST\n",component_id, stream_id);
        break;
    }
}

static gboolean check_sdp_file_cb(gpointer user_data)
{
    int sdp_file_fd = 0;
    char remote_sdp[4096] = {0};

    NiceAgent *agent = (NiceAgent *) user_data;

    sdp_file_fd = open("remote_sdp.txt", O_RDONLY);
    if (sdp_file_fd < 0)
        return TRUE;

    read(sdp_file_fd, remote_sdp, 4096);
    close(sdp_file_fd);
    unlink("remote_sdp.txt");

    printf("\nReceived remote SDP:\n");
    printf("%s\n", remote_sdp);

    nice_agent_parse_remote_sdp(agent, remote_sdp);
    return FALSE;
}

static void on_gathering_done_cb(NiceAgent * agent, guint stream_id, gpointer user_data)
{
    GSList * candidates, * i;
    candidates = nice_agent_get_local_candidates (agent, stream_id, 1);
    printf("Gathered %d candidates:\n", g_slist_length(candidates));

    for (i = candidates ; i ; i = i->next)
    {
        NiceCandidate *c = (NiceCandidate *) i->data;
        gchar * candidate_sdp = nice_agent_generate_local_candidate_sdp(agent, c);
        printf("\t%s\n", candidate_sdp);
        g_free((candidate_sdp));
    }

    printf("\nFull local SDP:\n");
    gchar * full_sdp = nice_agent_generate_local_sdp(agent);
    printf("%s\n", full_sdp);
    int local_sdp_fd = open("local_sdp.txt", O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
    if (local_sdp_fd > 0)
    {
        write(local_sdp_fd, full_sdp, strlen(full_sdp));
        close(local_sdp_fd);
    }
    else
    {
        fprintf(stderr, "Failed to open local sdp file: %s\n", strerror(errno));
    }
    g_free(full_sdp);

    sdp_file_check_timer = g_timeout_source_new(500);
    g_source_set_callback(sdp_file_check_timer, check_sdp_file_cb, agent, NULL);
    g_source_attach(sdp_file_check_timer, mainctx);
}


/********************************************************
 * MAIN PROGRAM
 ********************************************************/
int main(int argc, char ** argv)
{
    guint stream_id = 0;
    NiceAgent * agent = NULL;
    gboolean controlling = FALSE;

    mainctx = g_main_context_new();
    mainloop = g_main_loop_new(mainctx, FALSE);

    agent = nice_agent_new(mainctx, NICE_COMPATIBILITY_RFC5245);
    g_object_set(G_OBJECT(agent),
                 "stun-server", "66.102.1.127",
                 "stun-server-port", 19302,
                 NULL);
    /* 2 rôles: controlling and controlled.
       In a peer to peer session, 1 peer must be controlling and the other must be controlled.
       In WebRTC this rôle is dependant of who has sent the offer
    */
    if (argc > 1 && ! strcmp(argv[1], "--controlling"))
    {
        printf("Agent has controlling role\n");
        controlling = TRUE;
    }
    g_object_set(G_OBJECT(agent), "controlling-mode", controlling, NULL);

    // Creating a nice stream
    stream_id = nice_agent_add_stream(agent, 1);
    nice_agent_set_stream_name(agent, stream_id, "application");

    nice_agent_set_port_range(agent, stream_id, 1, 20000, 40000);

    // Listen Nice Agent events
    g_signal_connect(G_OBJECT(agent), "candidate-gathering-done",
                     G_CALLBACK(on_gathering_done_cb), NULL);
    g_signal_connect(G_OBJECT(agent), "component-state-changed",
                 G_CALLBACK(on_state_changed_cb), NULL);

    // Let's gather local candidates!
    nice_agent_gather_candidates(agent, stream_id);

    // MAIN LOOP
    g_main_loop_run(mainloop);
    g_main_loop_unref(mainloop);

    return 0;
}

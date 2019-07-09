#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <glib-object.h>

#include <agent.h>

/********************************************************
 * STATIC VARIABLES
 ********************************************************/
static GMainContext * mainctx = NULL;
static GMainLoop * mainloop = NULL;

static guint stream_id = 0;
static NiceAgent * agent = NULL;


/********************************************************
 * INTERNAL FUNCTIONS
 ********************************************************/
static void nice_data_cb(NiceAgent * agent, guint stream_id, guint component_id,
                           guint len, gchar * buf,
                           gpointer user_data)
{
    printf("Data received from nice agent. Don't process it.\n");
    return;
}

static void candidate_gathering_done_cb(void)
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

    g_main_loop_quit(mainloop);
}


/********************************************************
 * MAIN PROGRAM
 ********************************************************/
int main(void)
{
    mainctx = g_main_context_new();
    mainloop = g_main_loop_new(mainctx, FALSE);

    /********************
    agent = g_object_new(NICE_TYPE_AGENT,
                         "compatibility", NICE_COMPATIBILITY_DRAFT19,
                         "main-context", mainctx,
                         "reliable",  FALSE,
                         "full-mode", TRUE,
                         "ice-udp",   TRUE,
                         "ice-tcp",   FALSE);
    ********************/
    agent = nice_agent_new(mainctx, NICE_COMPATIBILITY_DRAFT19);
    g_object_set(G_OBJECT(agent),
                 "stun-server", "66.102.1.127",
                 "stun-server-port", 19302,
                 NULL);
    /* 2 rôles: controlling and controlled.
       In a peer to peer session, 1 peer must be controlling and the other must be controlled.
       In WebRTC this rôle is dependant of who has sent the offer
    */
    g_object_set(G_OBJECT(agent), "controlling-mode", TRUE, NULL);

    // Creating a nice stream
    stream_id = nice_agent_add_stream(agent, 1);
    nice_agent_set_stream_name(agent, stream_id, "video");

    nice_agent_set_port_range(agent, stream_id, 1, 20000, 40000);

    // Simple tests section

    g_signal_connect (G_OBJECT(agent), "candidate-gathering-done",
                  G_CALLBACK (candidate_gathering_done_cb), NULL);

    nice_agent_gather_candidates(agent, stream_id);
    nice_agent_attach_recv(agent, stream_id, 1, mainctx,
        nice_data_cb, NULL);

    // MAIN LOOP
    g_main_loop_run(mainloop);
    /* When the loop quits, we can unref it */
    g_main_loop_unref(mainloop);

    return 0;
}

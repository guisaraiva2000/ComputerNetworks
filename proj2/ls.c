/******************************************************************************\
* Link state routing protocol.                                                 *
\******************************************************************************/

#include <stdlib.h>
#include <stdio.h>


#include "routing-simulator.h"

#define min(a, b) (int) a < (int) b ? a:b

typedef struct {
  cost_t link_cost[MAX_NODES];
  int version;
} link_state_t;

// Message format to send between nodes.
typedef struct {
  link_state_t ls[MAX_NODES];
} message_t;

// State format.
typedef struct {
    link_state_t ls[MAX_NODES];
} state_t;


// Send messages from current_node to all neighbors
void send_msg(state_t *state, node_t current_node) {
    for (node_t i = get_first_node(); i <= get_last_node(); i++) {
        if (i != current_node && state->ls[current_node].link_cost[i] != COST_INFINITY) {
            message_t *nm = (message_t *) malloc(sizeof(message_t));

            for (node_t x = get_first_node(); x <= get_last_node(); x++) {
                nm->ls[x].version = state->ls[x].version;

                for (node_t y = get_first_node(); y <= get_last_node(); y++)
                    nm->ls[x].link_cost[y] = state->ls[x].link_cost[y];
            }
            send_message(i, nm);
        }
    }
}

// Check if there are new routes w Dijkstra
void find_routes(state_t *state, node_t current_node) {
    cost_t distances[MAX_NODES], path[MAX_NODES];
    int visited[MAX_NODES] = {0};

    for (node_t j = get_first_node(); j <= get_last_node(); j++) {
        distances[j] = state->ls[current_node].link_cost[j];
        path[j] = current_node;
    }

    visited[current_node] = 1;
    int min_index;

    for (node_t n = get_first_node(); n <= get_last_node(); n++) {

        cost_t minimum = COST_INFINITY;

        for (node_t v = get_first_node(); v <= get_last_node(); v++)
            if (!visited[v] && distances[v] <= minimum)
                minimum = distances[v], min_index = v;

        visited[min_index] = 1;

        for (node_t j = get_first_node(); j <= get_last_node(); j++)
            if (!visited[j] && minimum + state->ls[min_index].link_cost[j] < distances[j]) {
                distances[j] = minimum + state->ls[min_index].link_cost[j];
                path[j] = min_index;
            }
    }

    node_t j;
    for (node_t n = get_first_node(); n <= get_last_node(); n++) {
        if (n != current_node) {
            if (state->ls[current_node].link_cost[n] != distances[n]) {

                node_t min_path_link;
                cost_t min_path_cost = COST_INFINITY;

                j = n;
                while (1) {
                    j = path[j];
                    if (j == current_node) break;

                    if (state->ls[current_node].link_cost[j] < min_path_cost) {
                        min_path_cost = state->ls[current_node].link_cost[j];
                        min_path_link = j;
                    }
                }

                set_route(n, min_path_link, distances[n]);

            } else {
                set_route(n, n, distances[n]);
            }
        }
    }
}

// Notify a node that a neighboring link has changed cost.
void notify_link_change(node_t neighbor, cost_t new_cost) {
    int current_node = get_current_node();
    state_t *state = (state_t *) get_state();

    if (!state) { // initial state
        state = (state_t *) malloc(sizeof(state_t));
        set_state(state);
        for (node_t n = get_first_node(); n <= get_last_node(); n++) {
            for (node_t j = get_first_node(); j <= get_last_node(); j++) {
                if (!(j == current_node && n == current_node)) {
                    state->ls[n].link_cost[j] = COST_INFINITY;
                }
            }
        }
        state->ls[current_node].version = 0;
    }

    state->ls[current_node].version++;
    state->ls[current_node].link_cost[neighbor] = new_cost;

    find_routes(state, current_node);
    send_msg(state, current_node);
}

// Receive a message sent by a neighboring node.
void notify_receive_message(node_t sender, void *message) {
    int current_node = get_current_node(), latest_version = 0;
    state_t *state = (state_t *) get_state();
    message_t *m = (message_t *) message;

    for (node_t j = get_first_node(); j <= get_last_node(); j++) {
        if (state->ls[j].version < m->ls[j].version) {

            latest_version = 1;
            state->ls[j].version = m->ls[j].version;

            for (node_t n = get_first_node(); n <= get_last_node(); n++)
                state->ls[j].link_cost[n] = m->ls[j].link_cost[n];
        }
    }

    if (latest_version) { // Recent version

        find_routes(state, current_node);
        send_msg(state, current_node);

    }
}

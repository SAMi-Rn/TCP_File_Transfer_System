//
// Created by Sami Roudgarian on 2023-10-05.
//

#include "fsm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


static dc_fsm_state_func
fsm_transition(const struct dc_env *env, int from_id, int to_id, const struct dc_fsm_transition transitions[]);

struct dc_fsm_info
{
    char  *name;
    size_t name_length;
    int    from_state_id;
    int    current_state_id;

    void (*will_change_state)(const struct dc_env *env,
                              struct dc_error           *err,
                              const struct dc_fsm_info  *info,
                              int                        from_state_id,
                              int                        to_state_id);

    void (*did_change_state)(const struct dc_env *env,
                             struct dc_error           *err,
                             const struct dc_fsm_info  *info,
                             int                        from_state_id,
                             int                        to_state_id,
                             int                        next_id);

    void (*bad_change_state)(const struct dc_env *env,
                             struct dc_error           *err,
                             const struct dc_fsm_info  *info,
                             int                        from_state_id,
                             int                        to_state_id);
};

struct dc_fsm_info *dc_fsm_info_create(const struct dc_env *env, struct dc_error *err, const char *name)
{
    struct dc_fsm_info *info;

    DC_TRACE(env);
    info = dc_calloc(env, err, 1, sizeof(struct dc_fsm_info));

    if(dc_error_has_no_error(err))
    {
        info->from_state_id    = DC_FSM_INIT;
        info->current_state_id = DC_FSM_USER_START;
        info->name_length      = dc_strlen(env, name) + 1;
        info->name             = dc_malloc(env, err, info->name_length);

        if(dc_error_has_no_error(err))
        {
            dc_strcpy(env, info->name, name);
        }
        else
        {
            dc_free(env, info);
            info = NULL;
        }
    }

    return info;
}

const char *dc_fsm_info_get_name(const struct dc_fsm_info *info)
{
    return info->name;
}

void dc_fsm_info_destroy(const struct dc_env *env, struct dc_fsm_info **pinfo)
{
    struct dc_fsm_info *info;

    DC_TRACE(env);
    info = *pinfo;
    dc_free(env, info->name);
    dc_free(env, info);
    *pinfo = NULL;
}

void dc_fsm_info_set_will_change_state(struct dc_fsm_info *info,
                                       void (*notifier)(const struct dc_env *env,
                                                        struct dc_error           *err,
                                                        const struct dc_fsm_info  *info,
                                                        int                        from_state_id,
                                                        int                        to_state_id))
{
    info->will_change_state = notifier;
}

void dc_fsm_info_set_did_change_state(struct dc_fsm_info *info,
                                      void (*notifier)(const struct dc_env *env,
                                                       struct dc_error           *err,
                                                       const struct dc_fsm_info  *info,
                                                       int                        from_state_id,
                                                       int                        to_state_id,
                                                       int                        next_id))
{
    info->did_change_state = notifier;
}

void dc_fsm_info_set_bad_change_state(struct dc_fsm_info *info,
                                      void (*notifier)(const struct dc_env *env,
                                                       struct dc_error           *err,
                                                       const struct dc_fsm_info  *info,
                                                       int                        from_state_id,
                                                       int                        to_state_id))
{
    info->bad_change_state = notifier;
}

int dc_fsm_run(const struct dc_env     *env,
               struct dc_error               *err,
               struct dc_fsm_info            *info,
               int                           *from_state_id,
               int                           *to_state_id,
               void                          *arg,
               const struct dc_fsm_transition transitions[])
{
    int from_id;
    int to_id;

    DC_TRACE(env);

    from_id = info->from_state_id;
    to_id   = info->current_state_id;

    do
    {
        dc_fsm_state_func perform;
        int               next_id;

        // notify moving to
        if(info->will_change_state)
        {
            info->will_change_state(env, err, info, from_id, to_id);
        }

        perform = fsm_transition(env, from_id, to_id, transitions);

        if(perform == NULL)
        {
            char  *error_message;
            size_t error_message_size;

            if(from_state_id)
            {
                *from_state_id = from_id;
            }

            if(to_state_id)
            {
                *to_state_id = to_id;
            }

            // notify error
            if(info->bad_change_state)
            {
                info->bad_change_state(env, err, info, from_id, to_id);
            }

            error_message_size = (size_t)snprintf(NULL, 0, "Unknown state transition: %d -> %d ", from_id, to_id);
            error_message      = dc_malloc(env, err, error_message_size);
            sprintf(error_message, "Unknown state transition: %d -> %d ", from_id, to_id);  // NOLINT(cert-err33-c)
            DC_ERROR_RAISE_USER(err, error_message, 1);
            dc_free(env, error_message);

            return -1;
        }

        info->from_state_id    = from_id;
        info->current_state_id = to_id;
        from_id                = to_id;
        next_id                = perform(env, err, arg);

        // notify moving from
        if(info->did_change_state)
        {
            info->did_change_state(env, err, info, info->from_state_id, info->current_state_id, next_id);
        }

        to_id = next_id;

        if(dc_error_has_error(err))
        {
            // sometimes states do want to pass errors along... maybe?
            // how do we sort out an internal error and an error from a state?
            // we had an issue that we can't cope with
            // break;
        }
    } while(to_id != DC_FSM_EXIT);

    // commenting this out will give us the last non-exit transition, probably more useful
    if(from_state_id)
    {
        *from_state_id = from_id;
    }

    if(to_state_id)
    {
        *to_state_id = to_id;
    }

    return 0;
}

static dc_fsm_state_func
fsm_transition(const struct dc_env *env, int from_id, int to_id, const struct dc_fsm_transition transitions[])
{
    const struct dc_fsm_transition *transition;

    DC_TRACE(env);
    transition = &transitions[0];

    while(transition->from_id != DC_FSM_IGNORE)
    {
        if(transition->from_id == from_id && transition->to_id == to_id)
        {
            return transition->perform;
        }

        transition = transitions++;
    }

    return NULL;
}
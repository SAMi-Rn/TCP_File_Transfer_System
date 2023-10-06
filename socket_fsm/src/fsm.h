//
// Created by Sami Roudgarian on 2023-10-05.
//

#ifndef SOCKET_FSM_FSM_H
#define SOCKET_FSM_FSM_H



/*
 * Copyright 2021-2021 D'Arcy Smith.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */





#ifdef __cplusplus
extern "C" {
#endif


struct dc_fsm_info;
struct dc_error;
struct dc_env;
typedef enum {
    DC_FSM_IGNORE = -1, // -1
    DC_FSM_INIT,        // 0
    DC_FSM_EXIT,        // 1
    DC_FSM_USER_START,  // 2
} dc_fsm_state;

typedef int (*dc_fsm_state_func)(const struct dc_env *env,
                                 struct dc_error *err, void *arg);

struct dc_fsm_transition {
    int from_id;
    int to_id;
    dc_fsm_state_func perform;
};

/**
 *
 * @param env
 * @param err
 * @param name
 * @return
 */
struct dc_fsm_info *dc_fsm_info_create(const struct dc_env *env,
                                       struct dc_error *err, const char *name);

/**
 *
 * @param env
 * @param pinfo
 */
void dc_fsm_info_destroy(const struct dc_env *env,
                         struct dc_fsm_info **pinfo);

/**
 *
 * @param info
 * @return
 */
const char *dc_fsm_info_get_name(const struct dc_fsm_info *info);

/**
 *
 * @param info
 * @param notifier
 */
void dc_fsm_info_set_will_change_state(
        struct dc_fsm_info *info,
        void (*notifier)(const struct dc_env *env, struct dc_error *err,
                         const struct dc_fsm_info *info, int from_state_id,
                         int to_state_id));

/**
 *
 * @param info
 * @param notifier
 */
void dc_fsm_info_set_did_change_state(
        struct dc_fsm_info *info,
        void (*notifier)(const struct dc_env *env, struct dc_error *err,
                         const struct dc_fsm_info *info, int from_state_id,
                         int to_state_id, int next_id));

/**
 *
 * @param info
 * @param notifier
 */
void dc_fsm_info_set_bad_change_state(
        struct dc_fsm_info *info,
        void (*notifier)(const struct dc_env *env, struct dc_error *err,
                         const struct dc_fsm_info *info, int from_state_id,
                         int to_state_id));

/**
 *
 * @param env
 * @param err
 * @param info
 * @param from_state_id
 * @param to_state_id
 * @param arg
 * @param transitions
 * @return
 */
int dc_fsm_run(const struct dc_env *env, struct dc_error *err,
               struct dc_fsm_info *info, int *from_state_id, int *to_state_id,
               void *arg, const struct dc_fsm_transition transitions[]);

#define DC_TRACE(env) dc_env_trace((env), __FILE__, __func__, __LINE__)
#ifdef __cplusplus
}
#endif


#endif //SOCKET_FSM_FSM_H

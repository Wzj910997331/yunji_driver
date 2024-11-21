/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <cstdint>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <filesystem>

namespace axcl {

#define SINGLE_PROC_SHM_SIZE 128

class single_proc final
{
public:
    single_proc(const char *app, key_t key) {
        if ((m_shm_id = shmget(key, SINGLE_PROC_SHM_SIZE, IPC_CREAT | 0666)) < 0) {
            perror("shmget");
            exit(1);
        }

        m_shm_addr = (pid_t*)shmat(m_shm_id, 0, 0);
        if (m_shm_addr == (pid_t *)-1) {
            perror("shmat");
            exit(1);
        }

        pid_t pid_old = *m_shm_addr;
        pid_t pid_new = getpid();
        std::filesystem::path app_path = app;

        if (pid_old != 0) {
            if (kill(pid_old, 0) == 0) {
                printf("%s is already running, please try later.\n", app_path.filename().c_str());
                exit(0);
            }
        }

        if (!__sync_bool_compare_and_swap((pid_t *)m_shm_addr, pid_old, pid_new)) {
            printf("%s is already running, please try later.\n", app_path.filename().c_str());
            exit(0);
        } else {
            m_is_run = true;
        }
    }

    ~single_proc() {
        if (m_is_run) {
            if (m_shm_addr != (pid_t *)-1) {
                *(pid_t *)m_shm_addr = 0;
                shmdt(m_shm_addr);
            }
            if (m_shm_id != -1) {
                shmctl(m_shm_id, IPC_RMID, NULL);
            }
        }
    }

private:
    int32_t   m_shm_id{-1};
    pid_t *   m_shm_addr{(pid_t *)-1};
    bool      m_is_run{false};
};

}  // namespace axcl::smi
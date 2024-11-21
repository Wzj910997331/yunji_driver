/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#include "threadx.hpp"
#include <pthread.h>
#include <unistd.h>
#include <utility>
#include "os.hpp"

axcl::threadx::threadx(axcl::threadx&& t) noexcept {
    swap(t);
}

void axcl::threadx::swap(axcl::threadx& t) noexcept {
    std::swap(m_thread, t.m_thread);
    auto running = m_running.load();
    m_running.store(t.m_running.load());
    t.m_running.store(running);
    std::swap(m_func, t.m_func);
    std::swap(m_name, t.m_name);
    std::swap(m_policy, t.m_policy);
    std::swap(m_priority, t.m_priority);
    std::swap(m_tid, t.m_tid);
}

void axcl::threadx::entry(axcl::event* evt) {
    m_running = true;
    m_tid = gettid();
    pthread_setname_np(pthread_self(), m_name.c_str());

    if ((SCHED_FIFO == m_policy || SCHED_RR == m_policy) && m_priority > 0) {
        struct sched_param sch;
        sch.sched_priority = m_priority;
        pthread_setschedparam(pthread_self(), m_policy, &sch);
    }

    evt->set();

    m_func();
    m_running = false;
}

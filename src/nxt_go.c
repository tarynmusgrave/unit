
/*
 * Copyright (C) Max Romanov
 * Copyright (C) NGINX, Inc.
 */

#include <nxt_main.h>
#include <nxt_router.h>


static nxt_int_t nxt_go_init(nxt_task_t *task, nxt_common_app_conf_t *conf);

static nxt_int_t nxt_go_run(nxt_task_t *task,
                      nxt_app_rmsg_t *rmsg, nxt_app_wmsg_t *msg);

nxt_application_module_t  nxt_go_module = {
    0,
    NULL,
    nxt_string("go"),
    "*",
    nxt_go_init,
    nxt_go_run,
    NULL,
};


extern char  **environ;

nxt_inline nxt_int_t
nxt_go_fd_no_cloexec(nxt_task_t *task, nxt_socket_t fd)
{
    int  res, flags;

    if (fd == -1) {
        return NXT_OK;
    }

    flags = fcntl(fd, F_GETFD);

    if (nxt_slow_path(flags == -1)) {
        nxt_alert(task, "fcntl(%d, F_GETFD) failed %E", fd, nxt_errno);
        return NXT_ERROR;
    }

    flags &= ~FD_CLOEXEC;

    res = fcntl(fd, F_SETFD, flags);

    if (nxt_slow_path(res == -1)) {
        nxt_alert(task, "fcntl(%d, F_SETFD) failed %E", fd, nxt_errno);
        return NXT_ERROR;
    }

    return NXT_OK;
}


static nxt_int_t
nxt_go_init(nxt_task_t *task, nxt_common_app_conf_t *conf)
{
    char               *argv[2];
    u_char             buf[256];
    u_char             *p, *end;
    nxt_int_t          rc;
    nxt_port_t         *my_port, *main_port;
    nxt_runtime_t      *rt;
    nxt_go_app_conf_t  *c;

    rt = task->thread->runtime;

    main_port = rt->port_by_type[NXT_PROCESS_MAIN];
    my_port = nxt_runtime_port_find(rt, nxt_pid, 0);

    if (nxt_slow_path(main_port == NULL || my_port == NULL)) {
        return NXT_ERROR;
    }

    rc = nxt_go_fd_no_cloexec(task, main_port->pair[1]);
    if (nxt_slow_path(rc != NXT_OK)) {
        return NXT_ERROR;
    }

    rc = nxt_go_fd_no_cloexec(task, my_port->pair[0]);
    if (nxt_slow_path(rc != NXT_OK)) {
        return NXT_ERROR;
    }

    end = buf + sizeof(buf);

    p = nxt_sprintf(buf, end,
                    "%s;%uD;"
                    "%PI,%ud,%d,%d,%d;"
                    "%PI,%ud,%d,%d,%d%Z",
                    NXT_VERSION, my_port->process->init->stream,
                    main_port->pid, main_port->id, (int) main_port->type,
                    -1, main_port->pair[1],
                    my_port->pid, my_port->id, (int) my_port->type,
                    my_port->pair[0], -1);

    if (nxt_slow_path(p == end)) {
        nxt_alert(task, "internal error: buffer too small for NXT_GO_PORTS");

        return NXT_ERROR;
    }

    nxt_debug(task, "update NXT_GO_PORTS=%s", buf);

    rc = setenv("NXT_GO_PORTS", (char *) buf, 1);
    if (nxt_slow_path(rc == -1)) {
        nxt_alert(task, "setenv(NXT_GO_PORTS, %s) failed %E", buf, nxt_errno);

        return NXT_ERROR;
    }

    c = &conf->u.go;

    argv[0] = c->executable;
    argv[1] = NULL;

    (void) execve(c->executable, argv, environ);

    nxt_alert(task, "execve(%s) failed %E", c->executable, nxt_errno);

    return NXT_ERROR;
}


static nxt_int_t
nxt_go_run(nxt_task_t *task,
           nxt_app_rmsg_t *rmsg, nxt_app_wmsg_t *msg)
{
    return NXT_ERROR;
}

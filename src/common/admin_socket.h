// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2011 New Dream Network
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software 
 * Foundation.  See file COPYING.
 * 
 */

#ifndef CEPH_COMMON_ADMIN_SOCKET_H
#define CEPH_COMMON_ADMIN_SOCKET_H

#include "common/Cond.h"

class AdminSocket;
class CephContext;

#define CEPH_ADMIN_SOCK_VERSION "2"

class AdminSocketHook {
public:
  virtual bool call(std::string command, cmdmap_t &cmdmap, std::string format,
		    bufferlist& out) = 0;
  virtual ~AdminSocketHook() {}
};
class InspectHook;

class AdminSocket : public Thread
{
public:
  AdminSocket(CephContext *cct);
  ~AdminSocket() override;

  /**
   * register an admin socket command
   *
   * The command is registered under a command string.  Incoming
   * commands are split by space and matched against the longest
   * registered command.  For example, if 'foo' and 'foo bar' are
   * registered, and an incoming command is 'foo bar baz', it is
   * matched with 'foo bar', while 'foo fud' will match 'foo'.
   *
   * The entire incoming command string is passed to the registred
   * hook.
   *
   * @param command command string
   * @param cmddesc command syntax descriptor
   * @param hook implementaiton
   * @param help help text.  if empty, command will not be included in 'help' output.
   *
   * @return 0 for success, -EEXIST if command already registered.
   */
  int register_command(std::string command, std::string cmddesc, AdminSocketHook *hook, std::string help);

  /**
   * unregister an admin socket command.
   *
   * If a command is currently in progress, this will block until it
   * is done.  For that reason, you must not hold any locks required
   * by your hook while you call this.
   *
   * @param command command string
   * @return 0 on succest, -ENOENT if command dne.
   */
  int unregister_command(std::string command);

  bool init(const std::string &path);

  void chown(uid_t uid, gid_t gid);
  void chmod(mode_t mode);

  /**
   * register an inspection probe on admin socket command
   *
   * Inspection probe is a simplified admin command that
   * does not accept arguments. It extracts information in
   * from running instance and returns it in formatted form.
   * It is acceptable to register multiple probes under same command_path
   * but 'id' must be different.
   * In such case all registered probes are invoked, in undetermined order.
   *
   * @param command_path command string
   * @param id probe specific identification
   * @param function probe to invoke
   *
   * @return true for success, false otherwise
   */
  bool register_inspect(const std::string& command_path,
                        const std::string& id,
                        std::function<bool(Formatter*)> function);

  /**
   * unregister an inspection probe
   *
   * Remove previously registered probe.
   *
   * @param command_path command string
   * @param id probe specific identification
   *
   * @return true for success, false otherwise
   */

  bool unregister_inspect(const std::string& command_path,
                          const std::string& id);
private:
  AdminSocket(const AdminSocket& rhs);
  AdminSocket& operator=(const AdminSocket &rhs);

  void shutdown();

  std::string create_shutdown_pipe(int *pipe_rd, int *pipe_wr);
  std::string destroy_shutdown_pipe();
  std::string bind_and_listen(const std::string &sock_path, int *fd);

  void *entry() override;
  bool do_accept();

  CephContext *m_cct;
  std::string m_path;
  int m_sock_fd;
  int m_shutdown_rd_fd;
  int m_shutdown_wr_fd;

  bool in_hook;
  Cond in_hook_cond;
  Mutex m_lock;    // protects m_hooks, m_descs, m_help
  AdminSocketHook *m_version_hook, *m_help_hook, *m_getdescs_hook;

  std::map<std::string,AdminSocketHook*> m_hooks;
  std::map<std::string,std::string> m_descs;
  std::map<std::string,std::string> m_help;

  Mutex inspect_lock;
  std::map<std::string, InspectHook*> inspects;

  friend class AdminSocketTest;
  friend class HelpHook;
  friend class GetdescsHook;
};

#endif

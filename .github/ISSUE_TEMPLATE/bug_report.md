---
name: Bug report
about: Create a report to help us improve
title: ''
labels: bug
assignees: ''

---

**Describe the bug**
A clear and concise description of what the bug is.

**To Reproduce**
Steps to reproduce the behavior:
1. Using this example configuration...
2. Dial into conference using verto
3. Play my_problem_file.mp4 into conference
4. FreeSWITCH crashes

**Expected behavior**
A clear and concise description of what you expected to happen.

**Package version or git hash**
 - Version [e.g. 1.10.4]

**Trace logs**
Provide freeswitch logs w/ DEBUG and UUID logging enabled

**backtrace from core file**
If applicable, provide the full backtrace from the core file.
```
(gdb) set pagination off
(gdb) set logging file /tmp/backtrace.log
(gdb) set logging on
Copying output to /tmp/backtrace.log.
(gdb) bt
(gdb) bt full
(gdb) info threads
(gdb) thread apply all bt
(gdb) thread apply all bt full
(gdb) set logging off
Done logging to /tmp/backtrace.log.
(gdb) quit
```

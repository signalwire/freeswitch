---
name: Bug report
about: Create a report to help us improve

---

<!--
FreeSWITCH uses GitHub Issues only for bugs in the code or feature requests. Please use this template only for bug reports.

If you have questions about using FreeSWITCH or related to its configuration file, ask on mailing list or Slack

Please try to fill this template as much as possible for any issue. It helps the developers to troubleshoot the issue.

If there is no content to be filled in a section, the entire section can be removed.

You can delete the comments from the template sections when filling.

You can delete next line and everything above before submitting (it is a comment).
-->

### Description

<!--
Explain what you did, what you expected to happen, and what actually happened.
-->

### Troubleshooting

#### Reproduction

<!--
If the issue can be reproduced, describe how it can be done.
-->

#### Debugging Data

<!--
If you got a core dump, use gdb to extract troubleshooting data - full backtrace,
local variables and the list of the code at the issue location.

If you are familiar with gdb, feel free to attach more of what you consider to
be relevant.
-->

```
(paste your debugging data here)
```

#### Log Messages

<!--
Check the syslog file and if there are relevant log messages printed by FreeSWITCH, add them next, or attach to issue, or provide a link to download them (e.g., to a pastebin site).
-->

```
(paste your log messages here)
```

#### SIP Traffic

<!--
If the issue is exposed by processing specific SIP messages, grab them with ngrep or save in a pcap file, then add them next, or attach to issue, or provide a link to download them (e.g., to a pastebin site).
-->

```
(paste your sip traffic here)
```

### Possible Solutions

<!--
If you found a solution or workaround for the issue, describe it. Ideally, provide a pull request with a fix.
-->

### Additional Information

  * **FreeSWITCH Version**

```
(paste your output here)
```

* **Operating System**:

<!--
Details about the operating system, the type: Linux (e.g.,: Debian 8.4, Ubuntu 16.04, CentOS 7.1, ...), MacOS, xBSD, Solaris, ...;
Kernel details (output of `uname -a`)
-->

```
(paste your output here)
```

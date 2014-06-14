Test File Format
================

Test files must be a single line in the following format:

{*command*} `|` {*config*} `|` {*options*} `|` {*input*} `|` {*exit*}

that is five fields separated by the pipe (`|`) character
(with optional whitespace)
where:

+ *command* = command to execute (`wrap` or `wrapc`)
+ *config*  = name of config file to use or `/dev/null` for none
+ *options* = command-line options or blank for none
+ *input*   = name of file to wrap
+ *exit*    = expected exit status code

# disk_analyzer

 Create a daemon that analyzes the used disk space on a device based on a given path and build a shell program that allows the use of this functionality from the command line.
The daemon must analyze the occupied space recursively for each contained directory in the given path.
**The cli will be named "da" and will have the following commands:**

**(1)**    Create an analysis task starting from a root directory and a given priority

&nbsp;&nbsp;&nbsp;**(a)** priorities can vary between 0 - 2 and determine the importance of the task (0 - low, 2 - normal, 3 - high)

&nbsp;&nbsp;&nbsp;**(b)** a task request for a directory that is already being analysed should not create a new task

**(2)** Removal of a given task

**(3)** Pausing and restarting a task

**(4)** Check the state of a task (preparing, in progress, done)

**`Usage: da [OPTION]... [DIR]...`**

**`Analyze the space occupied by the directory at [DIR]`**

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;`-a, --add analyze a new directory path for disk usage`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;`-p, --priority set priority for the new analysis (works only with -a argument)`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;`-S, --suspend <id> suspend task with <id>`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;`-R, --resume <id> resume task with <id>`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;`-r, --remove <id> remove the analysis with the given <id>`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;`-i, --info <id> print status about the analysis with <id> (pending, progress, done)`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;`-l, --list list all analysis tasks, with their ID and the corresponding root` 

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;`-p, --print <id> print analysis report for those tasks that are "done"`

# How to run
Run the Makefile using make, then type alias da=./client. After this you can run either **./client [OPTION] ...** or **da [OPTION] ...**

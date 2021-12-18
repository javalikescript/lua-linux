The Lua linux module exposes functions from the Linux OS.

```lua
local linux = require('linux')
linux.signal("SIGPIPE", "SIG_IGN") -- Ignore SIGPIPE to avoid the default process termination
```

Lua linux is covered by the MIT license.

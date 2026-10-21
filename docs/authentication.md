# Authentication

Use your Pasqal Cloud account email, password and project UUID. The provider
uses Pasqal's password-realm authentication lifecycle and keeps access tokens in
memory. It refreshes authentication when expiry requires it; a manually copied
static API token is unnecessary.

## Core session parameters

```python
from mqt.core.qdmi import builtin_driver

device = builtin_driver.open_device(
    "pasqal.emu-free",
    username=email,
    password=password,
    custom3=project_uuid,
)
```

Obtain `email`, `password` and `project_uuid` securely in the calling
application, for example with Python's `getpass.getpass()` for the password. Do
not print them. Standard QDMI `AUTHURL` and `AUTHFILE` are available for
documented account/configuration cases; ordinary users should keep the default
Pasqal endpoints. The cloud account must have access to the project. A short
project identifier may need resolution to the full UUID through authenticated
project metadata.

The session snapshots configuration when initialized. Create a fresh session
after changing credentials. The same account/project is needed to retrieve
earlier jobs. Authentication failure does not justify retrying a submission
whose response was lost; see
[failure behavior](capabilities.md#submission-and-recovery).

For applications using QDMI `AUTHFILE`, the file is a private UTF-8 `key=value`
file. Recognized keys are `username`, `password`, `project_id`, `auth_endpoint`,
`token`, `client_id`, and `client_secret`. Blank lines and lines beginning with
`#` or `;` are ignored. Surrounding whitespace and quotes are trimmed; no escape
syntax is interpreted. Explicit session parameters take precedence. Prefer
direct `username`/`password` arguments for passwords whose surrounding
whitespace or quotes are significant.

Store an auth file outside the checkout, restrict it to its owner (for example,
`chmod 600 ~/.config/pasqal-qdmi.auth` on Unix), and pass its path as
`auth_file`:

```ini
username=YOUR_EMAIL
password=YOUR_PASSWORD
project_id=YOUR_PROJECT_UUID
```

```python
device = builtin_driver.open_device("pasqal.emu-free", auth_file=auth_path)
```

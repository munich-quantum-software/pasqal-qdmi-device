# Authentication

Use your Pasqal Cloud account email, password and project UUID. The provider
uses Pasqal's password-realm authentication lifecycle and keeps access tokens in
memory. It refreshes authentication when expiry requires it; a manually copied
static API token is unnecessary.

## Environment variables

Set these once in your application or development environment:

| Variable            | Value                              |
| ------------------- | ---------------------------------- |
| `PASQAL_EMAIL`      | Pasqal Cloud account email         |
| `PASQAL_PASSWORD`   | Account password                   |
| `PASQAL_PROJECT_ID` | Full UUID of an accessible project |

You can then open a session without repeating credentials:

```python
from mqt.core.qdmi import builtin_driver

device = builtin_driver.open_device("pasqal.emu-free")
```

Explicit session parameters override nonempty environment variables, which in
turn override the auth file. Empty variables do not override file values. Do not
put passwords in shell history or source files. Tests and documentation use
these same variables; see [service setup](maintenance.md#credentials).

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
Pasqal endpoints. The cloud account must have access to the project. Use the
full project UUID from Pasqal Cloud, not the abbreviated identifier displayed in
some views.

Create a fresh session after changing credentials. The same account/project is
needed to retrieve earlier jobs. Authentication failure does not justify
retrying a submission whose response was lost; see
[failure behavior](capabilities.md#submission-and-recovery).

For applications using QDMI `AUTHFILE`, the file is a private UTF-8 `key=value`
file. Recognized keys are `username`, `password`, `project_id`, `auth_endpoint`,
`token`, `client_id`, and `client_secret`. `auth_endpoint` and `AUTHURL` require
a complete HTTPS URL. Blank lines and lines beginning with `#` or `;` are
ignored. Surrounding whitespace and quotes are trimmed; no escape syntax is
interpreted. Prefer direct `username`/`password` arguments for passwords whose
surrounding whitespace or quotes are significant.

Store the file at `~/.pasqal/config` to discover it automatically, or choose a
path with `PASQAL_AUTHFILE` or the session's `auth_file` parameter. Keep it
outside the checkout and restrict it to its owner (`chmod 600` on Unix):

```ini
username=YOUR_EMAIL
password=YOUR_PASSWORD
project_id=YOUR_PROJECT_UUID
```

```python
device = builtin_driver.open_device("pasqal.emu-free", auth_file=auth_path)
```

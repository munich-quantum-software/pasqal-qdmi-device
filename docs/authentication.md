# Authentication

Use your Pasqal Cloud account email, password and project UUID. The provider uses Pasqal's password-realm authentication lifecycle and keeps access tokens in memory. It refreshes authentication when expiry requires it; a manually copied static API token is unnecessary.

## Secure local configuration

Run the repository's interactive configuration helper in a terminal:

```sh
python3 scripts/configure_credentials.py --email YOUR_EMAIL --project YOUR_PROJECT_ID
```

The helper captures the password without echo and writes external local configuration below `~/.config/codex/pasqal-qdmi-device/`, with directory mode `0700` and credential files `0600`. If email and project files already exist, omit those arguments to update only the password. Keep these files outside the checkout and any build, log, fixture or artifact directory. Do not pass passwords as command-line arguments.

The explicit free-emulator validation command reads these local files. In CI, the same command consumes `PASQAL_EMAIL`, `PASQAL_PASSWORD` and `PASQAL_PROJECT_ID`. These variables belong to the validation launcher; the native QDMI interface accepts standard `USERNAME`/`PASSWORD` parameters and the project custom field.

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

Obtain `email`, `password` and `project_uuid` securely in the calling application. Do not print them. Standard QDMI `AUTHURL` and `AUTHFILE` are available for documented account/configuration cases; ordinary users should keep the default Pasqal endpoints. The cloud account must have access to the project. A short project identifier may need resolution to the full UUID through authenticated project metadata.

The session snapshots configuration when initialized. Create a fresh session after changing credentials. The same account/project is needed to retrieve earlier jobs. Authentication failure does not justify retrying a submission whose response was lost; see [failure behavior](capabilities.md#submission-and-recovery).

For applications using QDMI `AUTHFILE`, the file is a private UTF-8 `key=value` file. Recognized keys are `username`, `password`, `project_id`, `auth_endpoint`, `token`, `client_id`, and `client_secret`. Blank lines and lines beginning with `#` or `;` are ignored. Surrounding whitespace and quotes are trimmed; no escape syntax is interpreted. Explicit session parameters take precedence. Prefer direct `username`/`password` arguments for passwords whose surrounding whitespace or quotes are significant. The secure local helper's individual files are read by the validation launcher; they are not themselves `AUTHFILE` files.

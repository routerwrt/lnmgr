# Security Model

lnmgr relies on standard Unix permissions for authorization.

Access to the control socket determines who may observe or mutate
the link graph.

lnmgr does not implement internal ACLs or per-node permissions.

```mermaid
flowchart LR

S((Start)) --> W{Water?}
W -->|yes| R[Run pump]
W -->|no| H(Hold)
R --> E((Stop))
H --> E
```

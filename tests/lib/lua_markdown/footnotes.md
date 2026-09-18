Text with a note[^first] and another one[^Second], then the first again[^first].

A missing note[^none] stays as written, and so does [^with space].

[^first]: The first note, with **bold** and a [link](https://x.org).
[^second]: The second note goes on
    over an indented line.

[^unused]: Never referenced, still shown.

```
[^code]: not a footnote inside code
[x]: not a definition either
```

Reference links: [full][Ref One], [collapsed][], [shortcut] and [unknown][nope].

[ref one]: https://one.example "Title"
[collapsed]: <https://two.example>
[shortcut]: https://three.example

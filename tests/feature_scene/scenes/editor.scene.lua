return {
  entities = {
    {name = "anchor", transform = {position = {1, 0, 0}}},
    {name = "actor", transform = {parent = "anchor"},
     mesh = {mesh = "builtin:cube"}},
  },
  script = "scripts/editor.lua",
}

return {
  entities = {
    {name = "anchor", transform = {position = {1, 0, 0}}},
    {name = "actor", transform = {parent = "anchor"},
     mesh = {mesh = "builtin:cube"}, script = {path = "scripts/actor.lua"}},
  },
  script = "scripts/alternate.lua",
}

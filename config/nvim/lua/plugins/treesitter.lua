-- File: lua/plugins/treesitter.lua
return {
  {
    "nvim-treesitter/nvim-treesitter",
    opts = function(_, opts)
      -- Qui solo i linguaggi "generici" non coperti altrove
      vim.list_extend(opts.ensure_installed, {
        "bash",
        "json",
        "yaml",
        "lua",
        "query",
      })
    end,
  },
}

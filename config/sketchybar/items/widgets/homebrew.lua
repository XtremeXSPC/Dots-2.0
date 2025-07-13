local icons = require("icons")
local colors = require("colors")
local settings = require("settings")

--[[ Widget for managing Homebrew updates ]]

-- Configuration
local CONFIG = {
  check_interval = 60,
  update_interval = 900,
  brew_path = "/opt/homebrew/bin/brew",
  terminal_app = "alacritty",
  timeout = 120,
  debug = false,
  hover_effect = true,
  widget_name = "widgets.brew",
  package_icon = icons.package or "📦"
}

-- Color threshold definitions
local THRESHOLDS = {
  { count = 0,  color = colors.grey },
  { count = 1,  color = colors.blue },
  { count = 5,  color = colors.yellow },
  { count = 10, color = colors.orange },
  { count = 15, color = colors.red }
}

-- Visual feedback colors
local FEEDBACK_COLORS = {
  updating = "#FF00FF00",
  success = "#FF55FF55",
  error = "#FFFF5555",
  loading = "#FFFFFF55"
}

-- Helper functions
local function debug_log(message)
  if CONFIG.debug then print("[BREW] " .. message) end
end
local function safe_exec(command)
  debug_log("Executing command: " .. command)
  sbar.exec(command)
end
local function start_event_provider()
  local command = string.format(
    "pkill -f 'brew_check' >/dev/null 2>&1; " ..
    "$CONFIG_DIR/helpers/event_providers/brew_check/bin/brew_check brew_update %d %d %s &",
    CONFIG.check_interval, 
    CONFIG.update_interval,
    CONFIG.debug and "--verbose" or ""
  )
  safe_exec(command)
end
local function get_color(count)
  count = tonumber(count) or 0
  local color = colors.grey
  for i = #THRESHOLDS, 1, -1 do
    if count >= THRESHOLDS[i].count then color = THRESHOLDS[i].color; break; end
  end
  return color
end

-- Start event provider (unchanged)
start_event_provider()

-- Main widget - always visible (unchanged)
local brew = sbar.add("item", CONFIG.widget_name, {
  position = "right",
  icon = {
    string = CONFIG.package_icon,
    color = colors.grey,
    font = { family = settings.font.icons, style = settings.font.style_map["Regular"], size = 10.0, },
    padding_right = 4,
  },
  label = {
    string = "?",
    font = { family = settings.font.numbers, style = settings.font.style_map["Bold"], size = 9.0, },
    color = colors.grey,
    align = "right", padding_right = 0, width = 0, y_offset = 4
  },
  padding_right = settings.paddings + 6,
  background = { height = 22, color = { alpha = 0 }, border_color = { alpha = 0 }, drawing = true, },
})

-- Subscribe to update event (modified for robustness)
brew:subscribe("brew_update", function(env)
  local count = tonumber(env.outdated_count) or 0
  local color = get_color(count)
  
  -- Ensure the icon is always set to prevent disappearing
  brew:set({
    icon = { string = CONFIG.package_icon, color = color },
    label = { string = tostring(count), color = color }
  })
  
  -- Tooltip logic (unchanged from original version)
  local tooltip = ""
  local error_message = env.error or ""
  if error_message ~= "" and error_message ~= "No error" then tooltip = "ERROR: " .. error_message .. "\n\n" end
  if count > 0 then
    tooltip = tooltip .. "Packages to update: " .. (env.pending_updates or "none")
    local last_check = tonumber(env.last_check) or 0
    if last_check > 0 then
      local time_diff = os.time() - last_check
      if time_diff < 60 then tooltip = tooltip .. string.format("\n\nLast check: %d seconds ago", time_diff)
      elseif time_diff < 3600 then tooltip = tooltip .. string.format("\n\nLast check: %d minutes ago", math.floor(time_diff / 60))
      else tooltip = tooltip .. string.format("\n\nLast check: %d hours ago", math.floor(time_diff / 3600)) end
    end
    tooltip = tooltip .. "\n\nLeft click: Show details\nRight click: Update all\nMiddle click: Force check"
  else
    tooltip = tooltip .. "All packages are up to date"
  end
  brew:set({ tooltip = tooltip })
end)

-- === NEW INTEGRATED AND ROBUST CLICK SCRIPT ===
brew:set({
  click_script = string.format([[
    #!/bin/bash
    # Robust, self-contained and reliable click controller.

    # --- Configuration ---
    WIDGET_NAME="%s"
    TERMINAL_APP="%s"
    PACKAGE_ICON="%s"
    BREW_PATH="%s"
    CHECK_PROCESS_NAME="brew_check"

    # --- Main Logic ---

    # 1. Instant and Safe Visual Feedback
    #    Set both icon and "loading" color to solve the problem
    #    of disappearing icon. Restoration will happen later.
    sketchybar --set "$WIDGET_NAME" icon="$PACKAGE_ICON" icon.color='#FFFFFF55'

    # --- Click Handling ---

    if [ "$BUTTON" = "middle" ]; then
        # Middle click: Send refresh signal to background process.
        pkill -USR1 -f "$CHECK_PROCESS_NAME"
        # After a brief moment, trigger an event to ensure UI restores.
        sleep 0.5
        sketchybar --trigger brew_update
        exit 0
    fi

    # For left and right clicks, execute all logic in a background subshell
    # (&) to never block Sketchybar's interface.
    (
      COUNT=$(sketchybar --query "$WIDGET_NAME" | jq -r '.label.value' 2>/dev/null || echo 0)

      # Determine the command to execute in terminal.
      task_command="'$BREW_PATH' outdated" # Default for left click
      if [ "$BUTTON" = "right" ]; then
        task_command="'$BREW_PATH' upgrade"
      fi
      if (( COUNT == 0 )); then
        task_command="echo '✅ Homebrew is already up to date.'"
      fi

      # 2. Launch Alacritty in background and capture its PID.
      #    This solves the problem of windows not closing.
      "$TERMINAL_APP" -e bash -c "$task_command; echo; read -p 'Press Enter to close...'" &
      TERMINAL_PID=$!

      # 3. Wait for the terminal process (and only that) to finish.
      #    This is a blocking call, but happens in a background subshell,
      #    so it doesn't freeze the bar.
      wait $TERMINAL_PID

      # 4. AFTER the terminal closes, send a signal to the C helper
      #    to force counter update.
      #    This solves the problem of counter not updating.
      pkill -USR1 -f "$CHECK_PROCESS_NAME"

    ) & # The final ampersand is crucial for UI responsiveness.

  ]], CONFIG.widget_name, CONFIG.terminal_app, CONFIG.package_icon, CONFIG.brew_path)
})

-- Hover effect and surrounding elements (unchanged)
if CONFIG.hover_effect then
  brew:subscribe("mouse.entered", function(env) brew:set({ background = { color = colors.bg2 }}) end)
  brew:subscribe("mouse.exited", function(env) brew:set({ background = { color = { alpha = 0 } }}) end)
end
sbar.add("bracket", CONFIG.widget_name .. ".bracket", { brew.name }, { background = { color = colors.bg1 }})
sbar.add("item", CONFIG.widget_name .. ".padding", { position = "right", width = settings.group_paddings })

-- Initial trigger (unchanged)
sbar.exec("sketchybar --trigger brew_update")

debug_log("Homebrew widget initialized successfully")

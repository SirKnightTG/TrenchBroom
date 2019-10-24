set(MANUAL_MENU_PATTERN       	"#menu\\(([^)]+)\\)")
set(MANUAL_MENU_REPLACEMENT   	"<script>print_menu_item(\"\\1\");</script>")
set(MANUAL_ACTION_PATTERN     	"#action\\(([^)]+)\\)")
set(MANUAL_ACTION_REPLACEMENT	"<script>print_action(\"\\1\");</script>")
set(MANUAL_KEY_PATTERN 	    	"#key\\(([0-9]+)\\)")
set(MANUAL_KEY_REPLACEMENT 		"<script>print_key(\\1);</script>")

file(READ "${INPUT}" MANUAL_CONTENTS)
string(REGEX REPLACE "${MANUAL_MENU_PATTERN}"   "${MANUAL_MENU_REPLACEMENT}"   MANUAL_CONTENTS "${MANUAL_CONTENTS}")
string(REGEX REPLACE "${MANUAL_ACTION_PATTERN}" "${MANUAL_ACTION_REPLACEMENT}" MANUAL_CONTENTS "${MANUAL_CONTENTS}")
string(REGEX REPLACE "${MANUAL_KEY_PATTERN}"    "${MANUAL_KEY_REPLACEMENT}"    MANUAL_CONTENTS "${MANUAL_CONTENTS}")
file(WRITE "${OUTPUT}" "${MANUAL_CONTENTS}")

# -*- coding: utf-8 -*-
#
# WeeChat Python script for Wraith IRC bot authentication
# Based on the irssi and mIRC wraith authentication scripts
#
# Usage:
#   1. Copy this script to ~/.weechat/python/autoload/
#   2. Load it in WeeChat: /python load wraith_auth.py
#   3. Configure settings: /set plugins.var.python.wraith_auth.auth_secpass <value>
#   4. Configure settings: /set plugins.var.python.wraith_auth.auth_authkey <value>
#   5. Use /wraithauth -Auth <challenge> to authenticate
#
# This script handles:
#   - SHA256 hash computation for authentication (default; MD5 accepted for compat)
#   - Private message (-Auth) handling
#   - DCC chat (-Auth) handling
#   - Settings management for auth credentials
#   - Command /wraithauth for manual hash generation

import weechat
import hashlib
import re

SCRIPT_NAME = "wraith_auth"
SCRIPT_AUTHOR = "fred"
SCRIPT_VERSION = "1.06"
SCRIPT_LICENSE = "GPL"
SCRIPT_DESC = "Private auth script for botpack wraith"

# Script registration
weechat.register(
    SCRIPT_NAME,
    SCRIPT_AUTHOR,
    SCRIPT_VERSION,
    SCRIPT_LICENSE,
    SCRIPT_DESC,
    "",  # shutdown_function (none)
    ""   # charset (use default)
)

# Register settings with defaults
if not weechat.config_is_set_plugin("auth_secpass"):
    weechat.config_set_plugin("auth_secpass", "")
if not weechat.config_is_set_plugin("auth_authkey"):
    weechat.config_set_plugin("auth_authkey", "")
if not weechat.config_is_set_plugin("auth_password"):
    weechat.config_set_plugin("auth_password", "")
if not weechat.config_is_set_plugin("auth_hash_algorithm"):
    weechat.config_set_plugin("auth_hash_algorithm", "sha256")


def compute_auth_hash(auth_string):
    """Compute SHA256 or MD5 hash for authentication.

    Defaults to SHA256 (64 hex chars). Set auth_hash_algorithm to "md5"
    for 32-char MD5 (old bot compat). The bot accepts both formats.
    """
    secpass = weechat.config_get_plugin("auth_secpass")
    authkey = weechat.config_get_plugin("auth_authkey")
    algorithm = weechat.config_get_plugin("auth_hash_algorithm") or "sha256"

    if not secpass:
        weechat.prnt("", "[wraith_auth] ERROR: auth_secpass is not set")
        return None
    if not authkey:
        weechat.prnt("", "[wraith_auth] ERROR: auth_authkey is not set")
        return None

    data = auth_string + secpass + authkey
    if algorithm == "md5":
        hash_obj = hashlib.md5(data.encode('utf-8'))
    else:
        hash_obj = hashlib.sha256(data.encode('utf-8'))
    return hash_obj.hexdigest()


def send_auth_request(buffer):
    """Send auth? to the nick of the current query buffer."""
    if weechat.buffer_get_string(buffer, "localvar_type") != "private":
        return False
    nick = weechat.buffer_get_string(buffer, "short_name")
    if not nick:
        return False
    server = weechat.buffer_get_string(buffer, "localvar_server")
    server_buffer = weechat.buffer_search("irc", f"server.{server}") if server else None
    if not server_buffer:
        return False
    weechat.command(server_buffer, f"/msg {nick} auth?")
    weechat.prnt(buffer, f"[wraith_auth] Sent auth request to {nick}")
    return True


def cmd_auth(data, buffer, args):
    """Handle /wraithauth command.

    Usage: /wraithauth -Auth <challenge>
           /wraithauth login   (in bot's query buffer)
    Returns the computed SHA256/MD5 authentication hash
    """
    if not args:
        if send_auth_request(buffer):
            return weechat.WEECHAT_RC_OK
        weechat.prnt(
            buffer,
            "Usage: /wraithauth -Auth <challenge>\n"
            "Configure with:\n"
            "  /set plugins.var.python.wraith_auth.auth_secpass <value>\n"
            "  /set plugins.var.python.wraith_auth.auth_authkey <value>\n"
            "  /set plugins.var.python.wraith_auth.auth_hash_algorithm sha256|md5"
        )
        return weechat.WEECHAT_RC_OK

    if args == "login":
        if send_auth_request(buffer):
            return weechat.WEECHAT_RC_OK
        weechat.prnt(buffer, "Usage: /wraithauth login (use in bot's query buffer)")
        return weechat.WEECHAT_RC_OK

    if args == "show":
        return show_auth_settings(data, buffer, args)

    # Extract -Auth from command (only the random string, not the bot nick)
    match = re.match(r'^-Auth\s+(\S+)', args)
    if not match:
        weechat.prnt(buffer, "Usage: /wraithauth -Auth <challenge>")
        return weechat.WEECHAT_RC_OK

    auth_string = match.group(1)
    result_hash = compute_auth_hash(auth_string)

    if result_hash:
        weechat.prnt(buffer, f"+Auth {result_hash}")

    return weechat.WEECHAT_RC_OK


def handle_privmsg_signal(data, signal, signal_data):
    """Handle raw private messages.

    Handles:
      - auth. / auth! (step 1: bot asks for password)
      - -Auth (step 2: bot sends hash challenge)
    """
    # signal_data format: "server;nick;address;text"
    parts = signal_data.split(";")
    if len(parts) < 4:
        return weechat.WEECHAT_RC_OK

    nick = parts[1]
    text = parts[3]

    # Skip if encrypted (starts with +OK) - let print hook handle it
    if text.startswith("+OK "):
        return weechat.WEECHAT_RC_OK

    msg = text.strip()

    # Handle special encoding (like \xff\xf9\x01 in mIRC)
    if msg.startswith('\xff\xf9\x01'):
        msg = msg[3:]

    # Check for auth. or auth! (bot requesting password)
    if msg.startswith('auth.') or msg.startswith('auth!'):
        password = weechat.config_get_plugin("auth_password")
        if password:
            server = parts[0]
            server_buffer = weechat.buffer_search("irc", f"server.{server}")
            if server_buffer:
                weechat.command(server_buffer, f"/msg {nick} auth {password}")
        return weechat.WEECHAT_RC_OK

    # Check for -Auth command (hash challenge)
    if re.match(r'^-Auth\s+', msg):
        match = re.match(r'^-Auth\s+(\S+)', msg)
        if match:
            auth_string = match.group(1)
            result_hash = compute_auth_hash(auth_string)

            if result_hash:
                server = parts[0]
                server_buffer = weechat.buffer_search("irc", f"server.{server}")

                if server_buffer:
                    weechat.command(server_buffer, f"/msg {nick} +Auth {result_hash}")
                    weechat.prnt(
                        "",
                        f"[wraith_auth] Auto-sent +Auth response to {nick}"
                    )
                else:
                    weechat.prnt(
                        "",
                        f"[wraith_auth] ERROR: Could not find server buffer for {server}"
                    )

    return weechat.WEECHAT_RC_OK


def handle_printed_message(data, buffer, date, tags, displayed, highlight, prefix, message):
    """Handle messages printed to buffers (after FiSH decryption).

    This hook fires when messages are printed to the buffer, AFTER FiSH plugin
    has decrypted them. This ensures we see decrypted -Auth challenges.
    """
    # If message is not provided, can't process
    if not message:
        return weechat.WEECHAT_RC_OK

    # Only process private message buffers (DCC or query)
    buffer_type = weechat.buffer_get_string(buffer, "localvar_type")
    if buffer_type not in ("private", ""):
        return weechat.WEECHAT_RC_OK

    # Get nick from buffer name
    buffer_name = weechat.buffer_get_string(buffer, "short_name")
    if not buffer_name:
        return weechat.WEECHAT_RC_OK

    msg = message.strip()

    # Handle special encoding (like \xff\xf9\x01 in mIRC)
    if msg.startswith('\xff\xf9\x01'):
        msg = msg[3:]

    # Check for auth. or auth! (bot requesting password)
    if msg.startswith('auth.') or msg.startswith('auth!'):
        password = weechat.config_get_plugin("auth_password")
        if password:
            server = weechat.buffer_get_string(buffer, "localvar_server")
            server_buffer = weechat.buffer_search("irc", f"server.{server}") if server else None
            if server_buffer:
                weechat.command(server_buffer, f"/msg {buffer_name} auth {password}")
        return weechat.WEECHAT_RC_OK

    # Check for -Auth command
    if re.match(r'^-Auth\s+', msg):
        # Extract the challenge string (first word only, not bot nick)
        match = re.match(r'^-Auth\s+(\S+)', msg)
        if match:
            auth_string = match.group(1)
            result_hash = compute_auth_hash(auth_string)

            if result_hash:
                # Get the server for this buffer
                server = weechat.buffer_get_string(buffer, "localvar_server")
                server_buffer = weechat.buffer_search("irc", f"server.{server}") if server else None

                if server_buffer:
                    weechat.command(server_buffer, f"/msg {buffer_name} +Auth {result_hash}")
                    weechat.prnt(
                        "",
                        f"[wraith_auth] Auto-sent +Auth response to {buffer_name}"
                    )
                else:
                    weechat.prnt(
                        "",
                        f"[wraith_auth] ERROR: Could not find server buffer for {server}"
                    )

    return weechat.WEECHAT_RC_OK


def show_auth_settings(data, buffer, args):
    """Display current authentication settings (similar to /showauth in mIRC).

    Usage: /wraithauth show
    Shows the current values of auth_secpass and auth_authkey
    """
    secpass = weechat.config_get_plugin("auth_secpass") or "(not set)"
    authkey = weechat.config_get_plugin("auth_authkey") or "(not set)"
    password = weechat.config_get_plugin("auth_password") or "(not set)"
    algorithm = weechat.config_get_plugin("auth_hash_algorithm") or "sha256"

    weechat.prnt(buffer, "")
    weechat.prnt(buffer, "=== Wraith Auth Settings ===")
    weechat.prnt(buffer, f"  Password:  {password}")
    weechat.prnt(buffer, f"  SecPass:   {secpass}")
    weechat.prnt(buffer, f"  AuthKey:   {authkey}")
    weechat.prnt(buffer, f"  Algorithm: {algorithm}")
    weechat.prnt(buffer, "")

    return weechat.WEECHAT_RC_OK


# Register command
weechat.hook_command(
    "wraithauth",
    "Compute SHA256 or MD5 authentication hash for Wraith IRC bot",
    "[-Auth <challenge>] | [show]",
    "Arguments:\n"
    "  -Auth <challenge>  Compute hash for the bot's -Auth challenge string\n"
    "  show               Show current authentication settings\n\n"
    "Settings to configure:\n"
    "  /set plugins.var.python.wraith_auth.auth_secpass <value>\n"
    "  /set plugins.var.python.wraith_auth.auth_authkey <value>\n"
    "  /set plugins.var.python.wraith_auth.auth_password <value>\n"
    "  /set plugins.var.python.wraith_auth.auth_hash_algorithm sha256|md5\n\n"
    "Examples:\n"
    "  /wraithauth -Auth somestring\n"
    "  /wraithauth show",
    "show",
    "cmd_auth",
    ""
)

# Register hooks for message handling
# Signal hook: Catches unencrypted -Auth messages and skips encrypted ones
weechat.hook_signal("irc_server_privmsg", "handle_privmsg_signal", "")

# Print hook: Catches -Auth after FiSH decryption (if FiSH plugin is installed)
weechat.hook_print("", "", "", 1, "handle_printed_message", "")

# Print load message
weechat.prnt("", "Wraith authorization script loaded.")

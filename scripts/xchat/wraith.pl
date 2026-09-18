#!/usr/bin/perl -w

use strict;
use warnings;
use Digest::MD5 qw(md5_hex);
use Digest::SHA qw(sha256_hex);
use Encode qw(encode_utf8);

my $script = 'wraith-auth';
my $ver    = '1.1';

IRC::register($script, $ver, 'Wraith auth script for botpack wraith');


sub get_setting {
    my ($key, $default) = @_;
    my $val = IRC::get_pluginpref($key);
    return defined $val ? $val : $default;
}


sub compute_auth_hash {
    my ($auth_string) = @_;
    my $secpass = get_setting('auth_secpass', '');
    my $authkey = get_setting('auth_authkey', '');
    my $algo    = get_setting('auth_hash_algorithm', 'sha256');

    if (!$secpass) {
        IRC::print('[wraith_auth] ERROR: auth_secpass is not set');
        return undef;
    }
    if (!$authkey) {
        IRC::print('[wraith_auth] ERROR: auth_authkey is not set');
        return undef;
    }

    my $data = encode_utf8($auth_string . $secpass . $authkey);
    return $algo eq 'md5' ? md5_hex($data) : sha256_hex($data);
}


sub cmd_auth {
    my ($data, $server, $witem) = @_;
    if (!$data || $data !~ /^\-Auth\s+(\S+)/) {
        IRC::print("Usage: /auth -Auth <challenge>");
        return;
    }
    my $result = compute_auth_hash($1);
    IRC::print("+Auth $result") if $result;
}


sub show_auth {
    my $secpass = get_setting('auth_secpass', '(not set)');
    my $authkey = get_setting('auth_authkey', '(not set)');
    my $password = get_setting('auth_password', '(not set)');
    my $algo    = get_setting('auth_hash_algorithm', 'sha256');

    IRC::print('=== Wraith Auth Settings ===');
    IRC::print("  Password:  $password");
    IRC::print("  SecPass:   $secpass");
    IRC::print("  AuthKey:   $authkey");
    IRC::print("  Algorithm: $algo");
}


sub on_privmsg {
    my ($data) = @_;
    return unless $data =~ /^:(\S+?)!\S+ PRIVMSG (\S+) :(.+)$/;
    my ($nick, $target, $text) = ($1, $2, $3);
    return if $target =~ /^[#&]/;  # skip channels

    $text =~ s/^\xff\xf9\x01//;

    if ($text =~ /^auth\./ || $text =~ /^auth!/) {
        my $password = get_setting('auth_password', '');
        if ($password) {
            IRC::command("MSG $nick auth $password");
        }
    } elsif ($text =~ /^\-Auth\s+(\S+)/) {
        my $challenge = $1;
        my $result = compute_auth_hash($challenge);
        if ($result) {
            IRC::command("MSG $nick +Auth $result");
        }
    }
}


IRC::add_command('auth', \&cmd_auth, 'Compute wraith auth hash');
IRC::add_command('showauth', \&show_auth, 'Show auth settings');
IRC::hook_server('PRIVMSG', \&on_privmsg);

IRC::print('Wraith authorization script loaded.');

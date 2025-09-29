#!/usr/bin/env python3
import os
import re
import subprocess
import sys
import tempfile


def read_reply(proc):
    # GTP replies end with a blank line
    lines = []
    while True:
        line = proc.stdout.readline()
        if not line:
            break
        line = line.decode('utf-8', errors='replace')
        lines.append(line)
        if line.strip() == '':
            break
    return ''.join(lines)


def send(proc, cmd):
    proc.stdin.write((cmd + "\n").encode('utf-8'))
    proc.stdin.flush()
    return read_reply(proc)


def expect_ok(reply):
    assert reply.startswith('='), f"Expected OK reply, got: {reply!r}"


def expect_fail(reply):
    assert reply.startswith('?'), f"Expected failure reply, got: {reply!r}"


def main():
    if len(sys.argv) < 2:
        print("Usage: GTPIntegration_test.py <engine_binary>")
        sys.exit(2)
    binary = sys.argv[1]
    assert os.path.exists(binary), f"Binary not found: {binary}"

    env = os.environ.copy()
    proc = subprocess.Popen(
        [binary],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
    )

    try:
        # Basic identity
        reply = send(proc, 'protocol_version')
        expect_ok(reply)
        assert '2' in reply

        reply = send(proc, 'name')
        expect_ok(reply)
        assert 'Tenuki' in reply

        reply = send(proc, 'version')
        expect_ok(reply)

        # Setup a 9x9 game and play a move
        expect_ok(send(proc, 'boardsize 9'))
        expect_ok(send(proc, 'clear_board'))
        expect_ok(send(proc, 'komi 7.5'))

        # genmove should return a vertex like "A1" or "pass"
        reply = send(proc, 'genmove B')
        expect_ok(reply)
        move = reply.split('\n')[0].split(' ', 1)[-1].strip()
        assert move == 'pass' or re.match(r'^[A-HJ](?:[1-9])$', move), f"Bad move: {move}"

        # showboard returns a board with coordinates
        reply = send(proc, 'showboard')
        expect_ok(reply)
        assert 'A B C D E F G H J' in reply

        # Play explicit moves and pass
        expect_ok(send(proc, 'play B D4'))
        expect_ok(send(proc, 'play W pass'))

        # Verify J9 is valid (skip I column)
        expect_ok(send(proc, 'play B J9'))

        # final_score returns a score string
        reply = send(proc, 'final_score')
        expect_ok(reply)
        score = reply.split('\n')[0].split(' ', 1)[-1].strip()
        assert score == '0' or re.match(r'^[BW]\+[0-9]+(\.[0-9])?$', score), f"Bad score: {score}"

        # Unknown command should fail
        expect_fail(send(proc, 'unknown_command_xyz'))

        expect_ok(send(proc, 'quit'))
    finally:
        try:
            proc.terminate()
        except Exception:
            pass
        try:
            proc.wait(timeout=2)
        except Exception:
            proc.kill()


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
import os
import random
import re
import subprocess
import sys


def read_reply(proc):
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


def main():
    if len(sys.argv) < 2:
        print("Usage: GTPFuzz_test.py <engine_binary>")
        sys.exit(2)
    binary = sys.argv[1]
    assert os.path.exists(binary), f"Binary not found: {binary}"

    rng = random.Random(1234)
    env = os.environ.copy()
    proc = subprocess.Popen(
        [binary],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
    )

    try:
        expect_ok(send(proc, 'protocol_version'))
        expect_ok(send(proc, 'name'))
        expect_ok(send(proc, 'version'))

        for _ in range(5):  # few rounds, keep runtime small
            size = rng.choice([5, 7, 9])
            expect_ok(send(proc, f'boardsize {size}'))
            expect_ok(send(proc, 'clear_board'))
            expect_ok(send(proc, 'komi 6.5'))

            # play a short game by alternating genmove
            color = 'B'
            for i in range(30):
                reply = send(proc, f'genmove {color}')
                expect_ok(reply)
                move = reply.split('\n')[0].split(' ', 1)[-1].strip()
                assert move == 'pass' or re.match(r'^[A-HJ](?:[1-9])$', move), f"Bad move: {move}"
                color = 'W' if color == 'B' else 'B'

            # Quick sanity
            expect_ok(send(proc, 'final_score'))
            expect_ok(send(proc, 'showboard'))

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


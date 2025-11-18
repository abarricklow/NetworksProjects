#!/usr/bin/env python3

import socket
import subprocess
import os
import sys
import time
import requests
import queue
import threading
import random

HERE = os.path.dirname(os.path.realpath(__file__))

#if len(sys.argv) != 2 or not os.path.isdir(sys.argv[1]) or not os.path.join(sys.argv[1], "Makefile"):
#    print("Usage: ./autograder /path/to/student/makefile/directory")
#    exit(1)

#cmd = f"`cd {sys.argv[1]}; make server`"

# Start the server
#server_process = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

score = 0

# TEST 1: serve the home page 1000 times

address = "http://localhost:8080/"

num_sequential_requests = 1000

err_counter = 0
for i in range(num_sequential_requests):
    try:
        r = requests.get(address)
        assert r.status_code == 200
        assert "<html>" in r.text
        assert "</html>" in r.text
        assert "<body>" in r.text
        assert "</body>" in r.text
    except Exception as err:
        err_counter += 1

print(f"TEST 1: Of {num_sequential_requests} sequential requests, {err_counter} failed") 
score += (1 - (err_counter/num_sequential_requests)) * 30

# Test 2: serve the home page to 1000 concurrent requests
address = "http://localhost:8080/index.html"

num_concurrent_requests = 1000

def gethomepage(success_queue):
    address = "http://localhost:8080/index.html"
    try:
        #print("asking...")
        r = requests.get(address)
        assert r.status_code == 200
        assert "<html>" in r.text
        assert "</html>" in r.text
        assert "<body>" in r.text
        assert "</body>" in r.text
        success_queue.put(True)
    except Exception as err:
        #print(f"Failed to get homepage: {type(err)}: {err}\nStatus Code: {r.status_code}")
        success_queue.put(False)

success_queue = queue.Queue()
request_threads = []
for i in range(num_concurrent_requests):
    t = threading.Thread(target = gethomepage, args=(success_queue,))
    request_threads.append(t)
    t.daemon
    t.start()

for i in request_threads:
    i.join()

err_counter = 0
while True:
    try:
        if not success_queue.get(timeout=1):
            err_counter += 1
    except queue.Empty:
        break
print(f"TEST 2: Of {num_concurrent_requests} concurrent requests, {err_counter} failed") 
score += (1 - (err_counter/num_concurrent_requests)) * 30

# Test 3 -- Correctly Issue Error Conditions
errors = 0
successes = 0

tests = [
    ["/google", "POST", "405"],
    ["/google", "DELETE", "405"],
    ["/google", "OPTIONS", "405"],
    ["/multiply", "POST", "400"],
    ["/multiply", "GET", "405"],
    ["/database.php", "DELETE", "403"],
    ["/database.php", "GET", "405"],    
    ["/", "POST", "405"],
    ["/index.html", "POST", "405"],
    ["/favicon.ico", "GET", "404"],
]

def try_requesting(a):
    addr, m, c = a
    address = "http://localhost:8080" + addr
    try:
        if m == "POST":
            return requests.post(address).status_code    == int(c)
        elif m == "DELETE": 
            return requests.delete(address).status_code  == int(c)
        elif m == "GET":
            return requests.get(address).status_code     == int(c)
        elif m == "OPTIONS":
            return requests.options(address).status_code == int(c)
        else:
            assert False
    except:
        return False

for t in tests:
    if try_requesting(t):
        successes += 1
    else:
        print(f"Error! {t}")
        errors += 1

print(f"TEST 3: {successes} correct responses and {errors} incorrect error responses")
score += (successes / (errors + successes)) * 10


# Test 4 -- Correctly Redirect
errors = 0
successes = 0
address = "http://localhost:8080/google"

try:
    r = requests.get(address,allow_redirects=False)
    if r.status_code == 301:
        if r.headers["location"] == "https://google.com":
            successes += 1
        else:
            print("Error /google redirected to " + r.headers["location"])
            assert False
    else:
        print("GET /google status code was " + str(r.status_code))
        assert False
except Exception as err:
    errors += 1

print(f"TEST 4: {successes} correct responses and {errors} incorrect redirects")
score += (successes / (errors + successes)) * 10

# Test 5 -- Correctly Multiply
errors = 0
successes = 0

address = "http://localhost:8080/multiply"

for i in range(10):
    a = random.randint(-90,99)
    b = random.randint(-90,9999)
    answ = a*b
    r = requests.post(address,f"a={a}&b={b}")
    if str(answ) in r.text:
        successes += 1
    else:
        errors += 1

print(f"TEST 5: {successes} correct responses and {errors} incorrect /multiply requests")
score += (successes / (errors + successes)) * 20

# Test 6 -- Graduate Test -- Handle misbehaving clients...
errors = 0
successes = 0

def misbehave(q):
    a = random.randint(1,20)

    if a == 1:
    # Open a socket and never send anything for 10 seconds
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect(("127.0.0.1", 8080))
                time.sleep(10)
                s.close()
        except socket.error as err:
            #print(f"A1: {type(err)}: {err}")
            pass


    elif a == 2:
    # Open a socket and send one random byte at a time for 100 seconds
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.connect(("127.0.0.1", 8080))
                for i in range(400):
                   time.sleep(0.25)
                   s.send(open("/dev/random","rb").read(1))
                print("This server let me waste its time for 100 seconds!")
                s.close()
            except socket.error as err:
                #print(f"A2: {type(err)}: {err}")
                pass

    elif a == 3:
    # Open a socket and spam random bytes
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.connect(("127.0.0.1", 8080))
                for i in range(10):
                    time.sleep(0.25)
                    s.sendall(open("/dev/random","rb").read(100000000))
                s.close()
            except socket.error as err:
                #print(f"A3: {type(err)}: {err}")
                pass

    elif a == 4:
    # Open a socket and send a malformed HTTP request
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.connect(("127.0.0.1", 8080))
                s.sendall(bytes("""GET / HTTP/0.9\r\nContent-Length: 0\n\n\r""", encoding="ASCII"))
                s.close()
            except socket.error as err:
                #print(f"A4: {type(err)}: {err}")        
                pass

    elif a >= 5:
    # Send a valid request slowly
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.connect(("127.0.0.1", 8080))
                msg = (bytes("""GET / HTTP/1.1\r\nContent-Length: 0\r\n\r\n""", encoding="ASCII"))
                for b in msg:
                    time.sleep(0.1)
                    #print(f"sending {b}")
                    s.send(b.to_bytes(1, 'big'))
                s.settimeout(3)
                try:
                    data = s.recv(1024)
                    if b"200" in data.split(b"\r\n")[0]:
                        q.put(True)
                    else:
                        print("Error, slow request got an invalid response")
                        q.put(False)
                except socket.timeout:
                    print("Error, slow request was not responded to")
                    q.put(False)
                except socket.BrokenPipeError as err:
                    print("Error, student submission cut off slow sender")
                    q.put(False)
                try:
                    s.close()
                except socket.BrokenPipeError as err:
                    print("Slow sender couldn't close socket, was closed by server")
                    q.put(False)
            except socket.error as err:
                print(f"Slow sender error: {type(err)}: {err}")      
                q.put(False)  


    return

success_queue = queue.Queue()
request_threads = []
for i in range(1000):
    if random.randint(0,9) in [1,2,3,4,5,6]:
        t = threading.Thread(target = misbehave, args=(success_queue,))
    else:
        t = threading.Thread(target = gethomepage, args=(success_queue,))
    request_threads.append(t)
    t.daemon
    t.start()

for i in request_threads:
    i.join()

while True:
    try:
        if not success_queue.get(timeout=1):
            errors += 1
        else:
            successes += 1
    except queue.Empty:
        break

grad_score = (successes / (errors + successes)) * 100
print(f"TEST 6 (GRAD ONLY) {successes} correct responses and {errors} incorrect responses while handling misbehaving clients")

# Report score:
print(f"UNDERGRAD SCORE: {round(score,2)}/100")
print(f"GRADUATE  SCORE: {round(((score*0.8)+(grad_score*0.2)),2)}/100")

# Cleanup
#server_process.kill()




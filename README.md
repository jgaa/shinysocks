# ShinySOCKS

[![CI](https://github.com/jgaa/shinysocks/actions/workflows/ci.yaml/badge.svg?branch=master)](https://github.com/jgaa/shinysocks/actions/workflows/ci.yaml)

## Mission Statement

*To create a small, ultra-fast SOCKS proxy server that just works.*

## Background

I sometimes use VPN. In one case, I had VPN access only
from a Windows 7 virtual machine trough some proprietary
"security by obscurity", obscenely expensive enterprise
VPN software. In order to work
efficiently, I needed to connect my Linux workstation to that
VPN. Network routing and IP forwarding seems not to work,
so the second best option in my case is SOCKS. Socks
trough Putty works, kind of. It's slow and unreliable.

I tried a few free SOCKS servers. Neither of them worked, so
therefore I'm spending a few hours writing my own.

## Blog posts mentioning the project
- [My blog](https://lastviking.eu/_tags/shinysocks.html)

## Current State
The project has been in maintenance mode for ages. It just works.
However, these days I'm fixing some build issues to make it 
simpler for you or other hackers to submit pull requests with new features ;)

I'm also in the process of updating the code from mostly C++11 to C++20. But
it's not a very high priority - after all, the application work well as it is.

The SOCKS server works for SOCKS 4, 4a and 5 under
Linux, Windows and MacOS.

IPv6 and binding (reverse connections) are not yet supported.

## How I use it

I start ShinySOCKS on the command-line (cmd.exe) in a Windows or Linux
VM with VPN. Then I ssh to whatever servers on the VPN network
I desire - using ShinySOCKS as a proxy.

From Linux:
 $ ssh -o ProxyCommand='nc -x 192.168.0.10:1080 %h %p' jgaa@cool-server

For accessing intranet web pages over the VPN, I some times
used the [Foxy Proxy](https://getfoxyproxy.org/) Firefox plugin.
It simplifies things, and make my work-flow smooth. This also
improved my privacy, as the VPN host will only see the web traffic
going to the intranet sites.

## How I test it

If it run locally, and I have curl installed, I can test it like this:
```sh
curl -L --socks5-hostname socks5://localhost:1080 https://raw.githubusercontent.com/jgaa/shinysocks/master/ci/test.txt

```

## Docker

You can also pull a [Docker image](https://hub.docker.com/r/jgaafromnorth/shinysocks/)
with the server from Docker Hub.

```sh
docker pull jgaafromnorth/shinysocks
```

To run it.
```sh
docker run --rm --name shiny -p 1080:1080 -d jgaafromnorth/shinysocks
```

To  test it on the command-line with `curl`:
```sh
# Let curl do the DNS lookup
curl -L -x socks5://localhost:1080 https://www.google.com/

# Let shinysocks do the DNS lookup

curl -L --socks5-hostname socks5://localhost:1080 https://www.google.com/
```

You can now set the socks 5 address to ip `127.0.0.1` port `1080` in your applications (for example Firefox') proxy settings and test it.

## Command line

You can download a zipfile with the binary and run in from the command line:

**Linux**

After you have extracted the zip archive:

```sh
chmod +x shinysocks

./shinysocks -l debug -c ""

```

**Windows**

```sh

shinysocks -l debug -c ""

```

**MacOS**

```sh

chmod +x shinysocks

./shinysocks -l debug -c ""

```

To **disable logging**, set the log level to `none`.

## Supported platforms
- Linux
- Windows
- MacOS

## License
ShinySOCKS is released under GPLv3.

It is Free. Free as in free speech - at least in imaginary countries, where free speech is still allowed ;)


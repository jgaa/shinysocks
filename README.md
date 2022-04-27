# ShinySOCKS

## Mission Statement

*To create a small, ultrafast SOCKS proxy server.

## Background

I sometimes use VPN. In one case, I have a VPN access only
from a Windows 7 virtual machine trough some proprietary
"security by obscurity" VPN software. In order to work
efficiently, I need to connect my Linux workstation to that
VPN. Network routing and IP forwarding seems not to work,
so the second best option in my case is SOCKS. Socks
trough Putty works, kind of. It's slow and unreliable.

I tried a few free SOCKS servers. Neither of them worked, so
therefore I'm spending a few hours writing my own.

## Blog posts mentioning the project
- [My blog](https://lastviking.eu/_tags/shinysocks.html)

## Current State
The project is currently in maintainance mode. It just works.

The SOCKS server works for SOCKS 4, 4a and 5 under
Linux and Windows (compiled under Windows 7 with Visual
Studio 2015 RC and Boost 1.58). IPv6 and binding (reverse
connections) are not yet supported.

## How I use it

I start ShinySOCKS on the command-line (cmd.exe) in a Windows or Linux
VM with VPN. Then I ssh to whatever servers on the VPN network
I desire - using ShinySOCKS as a proxy.

From Linux:
 $ ssh -o ProxyCommand='nc -x 192.168.0.10:1080 %h %p' jgaa@cool-server

For accessing intranet web pages over the VPN, I some times
use the [Foxy Proxy](https://getfoxyproxy.org/) Firefox plugin.
It simplifies things, and make my work-flow smooth. This also
improves my privacy, as the VPN host will only see the web traffic
going to the intranet sites.

## Docker

You can also pull a [Docker image](https://hub.docker.com/r/jgaafromnorth/shinysocks/)
with the server from Docker Hub.

```
docker pull jgaafromnorth/shinysocks
```

## License
ShinySOCKS is released under GPLv3.
It is Free. Free as in speech. Free as in Free Air.

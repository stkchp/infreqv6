# infreqv6

## about

request DHCPv6 infomation to get DNS recursive server.

## usage

```.text
infreqv6 <ifname>

    <ifname> ... send dhcpv6 packet from this interface
```

example output:

```.text
2001:4860:4860::8888
2001:4860:4860::8844
```

## requirement

- Linux
	- gcc(5.1以降)
	- make


### tested environment

```.text
Gentoo Linux
    - gcc 9.3.0
    - make 4.3
```

## install

```.text
make
```

## license

- CC0 [![CC0](http://i.creativecommons.org/p/zero/1.0/88x31.png "CC0")](http://creativecommons.org/publicdomain/zero/1.0/deed.ja)

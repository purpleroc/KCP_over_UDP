package main

import (
	"log"

	"github.com/xtaci/kcp-go/v5"
)

func main() {
	log.Println("Start Server...")
	if listener, err := kcp.ListenWithOptions("0.0.0.0:12333", nil, 0, 0); err == nil {

		for {
			s, err := listener.AcceptKCP()
			s.SetNoDelay(1, 10, 2, 1)
			s.SetWindowSize(128, 128)
			if err != nil {
				log.Fatal(err)
			} else {
				log.Println("Got a client: ", s.RemoteAddr())
			}
			go handleEcho(s)
		}
	} else {
		log.Fatal(err)
	}
}

// handleEcho send back everything it received
func handleEcho(conn *kcp.UDPSession) {
	buf := make([]byte, 4096)
	randstr := "abcdefghijklmnopqrstuvwxyz"
	for {
		n, err := conn.Read(buf)
		if err != nil {
			log.Println(err)
			return
		} else {
			log.Println("recv: ", string(buf[:n]), n)
		}
		for i := 0; i < n; i++ {
			buf[i] = randstr[i%26]
		}
		n, err = conn.Write(buf[:n])
		if err != nil {
			log.Println(err)
			return
		} else {
			log.Println("send: ", string(buf[:n]), n)
		}
	}
}

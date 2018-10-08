// Golang scope experiment.
//
// In this example, 'x' is shared between the closure scope where it
// is modified, and the main scope where it is read.  'x' should
// increment with each printf, but it is not correctly synchronized
// (no mutex, not using channels). "go run -race" should warn
// about that.
// References,
// https://golang.org/ref/mem
// https://blog.golang.org/race-detector


package main

import (
	"log"
	"time"
)

func main() {
	var x int = 0

	go func () {
		for {
			time.Sleep(time.Second)
			x += 1
		}
	}()

	for {
		time.Sleep(time.Second)
		log.Printf("x=%d\n", x)
	}
}

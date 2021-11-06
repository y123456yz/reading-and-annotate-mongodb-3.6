package main

import (
	"flag"
	"fmt"
	"log"
	"os"
	"sort"
	"strings"
	"time"

	"gopkg.in/mgo.v2"
	"gopkg.in/mgo.v2/bson"
)

var events = []string{
	"total",
	"readLock",
	"writeLock",
	"queries",
	"insert",
	"update",
	"remove",
	"getmore",
	"commands",
}

type Stat map[string]int
type CollStat map[string]Stat
type Colls map[string]CollStat

type MgoTop struct {
	Totals Colls
	Ok     int
}

type Diff struct {
	Ns     string
	Counts map[string]int
	Sort   int
}

type ByDiff []Diff

func (b ByDiff) Len() int {
	return len(b)
}

func (b ByDiff) Swap(i, j int) {
	b[i], b[j] = b[j], b[i]
}

func (b ByDiff) Less(i, j int) bool {
	return b[i].Sort < b[j].Sort
}

func Calc(last, current *MgoTop, sortBy string, isSortByTime bool) (diffs ByDiff) {
	for collName, currentStat := range current.Totals {
		lastStat, ok := last.Totals[collName]
		if !ok {
			continue
		}
		var diff = Diff{
			Ns:     collName,
			Counts: make(map[string]int),
		}
		for _, event := range events {
			if isSortByTime {
				diff.Counts[event] = (currentStat[event]["time"] - lastStat[event]["time"]) / 1000
			} else {
				diff.Counts[event] = currentStat[event]["count"] - lastStat[event]["count"]
			}
			if sortBy[:4] == event[:4] {
				diff.Sort = diff.Counts[event]
			}
		}
		diffs = append(diffs, diff)
	}
	sort.Sort(sort.Reverse(diffs))
	return
}

func Show(diffs ByDiff, sortKey string, limit, back int, isSortByTime bool) (nextBack int) {
	if back != 0 {
		fmt.Printf("\033[%dA\r", back)
	}
	cond := "event count"
	if isSortByTime {
		cond = "time(ms)"
	}
	fmt.Printf("\033[1m====== mgotop ====== sort: %s %s ====== %s ======\033[m\n", sortKey, cond, time.Now().Format("2006-01-02T15:04:05"))
	fmt.Println("total\trlock\twlock\tquery\tinsert\tupdate\tremove\tgetmore\tcommand\tns")
	var i int
	for i = 0; i < limit && i < len(diffs); i++ {
		fmt.Print("\033[2K")
		for _, event := range events {
			fmt.Printf("%d\t", diffs[i].Counts[event])
		}
		fmt.Printf("%s\n", diffs[i].Ns)
	}
	return i + 2
}

func init() {
	go func() {
		// make sure user input will not effect display
		var b = make([]byte, 1)
		for {
			_, err := os.Stdin.Read(b)
			if err != nil {
				log.Fatal(err)
			}
			if b[0] == '\n' {
				fmt.Print("\033[1A\033[2K\r")
			}
		}
	}()
}

func main() {
	var (
		sortKey      = flag.String("k", "total", "sort key")
		isSortByTime = flag.Bool("t", false, "sort by used time?")
		isDirect     = flag.Bool("d", false, "direct connect to a slave node")
		limit        = flag.Int("n", 20, "show top n")
		sleepTime    = flag.Float64("s", 1, "sleep between each show")
		lastTop      *MgoTop
		host, port   string
		back         int
	)
	flag.StringVar(&host, "h", "127.0.0.1", "host")
	flag.StringVar(&host, "host", "127.0.0.1", "host")
	flag.StringVar(&port, "p", "27017", "port")
	flag.StringVar(&port, "port", "27017", "port")
	flag.Parse()
	if !strings.Contains(host, ":") {
		host += ":" + port
	}
	if *isDirect {
		host += "?connect=direct"
	}
	conn, err := mgo.Dial(host)
	if err != nil {
		log.Fatal(err)
	}
	if *isDirect {
		conn.SetMode(mgo.Eventual, true)
	}
	for {
		m := &MgoTop{}
		err = conn.DB("admin").Run(bson.M{"top": 1}, m)
		if err != nil {
			log.Fatal(err)
		}
		if m.Ok == 0 {
			log.Fatal(m)
		}
		if lastTop == nil {
			lastTop = m
			time.Sleep(time.Duration(*sleepTime*1000) * time.Millisecond)
			continue
		}
		diffs := Calc(lastTop, m, strings.ToLower(*sortKey), *isSortByTime)
		back = Show(diffs, *sortKey, *limit, back, *isSortByTime)
		lastTop = m
		time.Sleep(time.Duration(*sleepTime*1000) * time.Millisecond)
	}
}

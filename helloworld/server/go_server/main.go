package main

import (
	"context"
	"flag"
	"fmt"
	"io"
	"log"
	"net"

	"google.golang.org/grpc"
	"google.golang.org/grpc/channelz/service"
	pb "google.golang.org/grpc/examples/helloworld/helloworld"

	"time"
)

var (
	port = flag.Int("port", 50051, "The server port")
)

// server is used to implement helloworld.GreeterServer.
type server struct {
	pb.UnimplementedGreeterServer
}

// SayHello implements helloworld.GreeterServer
func (s *server) SayHello(ctx context.Context, in *pb.HelloRequest) (*pb.HelloReply, error) {
	log.Printf("Received: %v", in.GetName())
	return &pb.HelloReply{Message: "Hello " + in.GetName()}, nil
}

func (s *server) ComputeMean(stream pb.Greeter_ComputeMeanServer) error {
	var sum, total_numbers int32
	sum = 0
	total_numbers = 0
	start := time.Now()

	for {
		number, err := stream.Recv()
		if err == io.EOF {
			elapsed := time.Since(start)
			seconds_elapsed := elapsed.Seconds()
			log.Printf("Returning mean: %v", float32(sum)/float32(total_numbers))
			log.Printf("Time elapsed: %v seconds", seconds_elapsed)
			log.Printf("packets/sec: %v", float64(total_numbers)/seconds_elapsed)
			return stream.SendAndClose(&pb.FloatNumber{
				Value: float32(sum) / float32(total_numbers),
			})
		}
		if err != nil {
			return err
		}
		log.Printf("Received: %v", number.GetValue())
		sum += number.Value
		total_numbers++
	}
}

func (s *server) ComputeMeanStream(stream pb.Greeter_ComputeMeanStreamServer) error {
	var sum, total_numbers int32
	sum = 0
	total_numbers = 0
	start := time.Now()

	for {
		number, err := stream.Recv()
		if err == io.EOF {
			elapsed := time.Since(start)
			seconds_elapsed := elapsed.Seconds()
			log.Printf("Time elapsed: %v seconds", seconds_elapsed)
			log.Printf("packets/sec: %v", float64(total_numbers)/seconds_elapsed)
			return nil
		}
		if err != nil {
			return err
		}
		log.Printf("Received: %v", number.GetValue())
		sum += number.Value
		total_numbers++

		log.Printf("Returning mean: %v", float32(sum)/float32(total_numbers))
		if err := stream.Send(&pb.FloatNumber{
			Value: float32(sum) / float32(total_numbers),
		}); err != nil {
			return err
		}

	}
}

func main() {
	flag.Parse()
	lis, err := net.Listen("tcp", fmt.Sprintf("localhost:%d", *port))
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}
	s := grpc.NewServer()
	service.RegisterChannelzServiceToServer(s)
	pb.RegisterGreeterServer(s, &server{})
	log.Printf("server listening at %v", lis.Addr())
	if err := s.Serve(lis); err != nil {
		log.Fatalf("failed to serve: %v", err)
	}
}

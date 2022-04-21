package main

import (
	"context"
	"flag"
	"fmt"
	"io"
	"log"
	"net"

	"google.golang.org/grpc"
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

func (s *server) ComputeMeanRepeated(ctx context.Context, in *pb.FloatNumberList) (*pb.FloatNumber, error) {
	var sum float32
	var total_numbers int32
	var input []float32
	input = in.GetValue()
	for _, s := range input {
		sum += float32(s)
		total_numbers++
	}
	log.Printf("Returning mean: %v", float32(sum)/float32(total_numbers))
	return &pb.FloatNumber{
		Value: float32(sum) / float32(total_numbers),
	}, nil
}

func (s *server) SendLongString(ctx context.Context, in *pb.LongString) (*pb.LongString, error) {
	return &pb.LongString{Str: in.Str}, nil
}

func (s *server) ComputeMeanOrSendLongString(stream pb.Greeter_ComputeMeanOrSendLongStringServer) error {
	var sum float32
	var total_numbers int32
	var long_string string
	sum = 0
	total_numbers = 0

	for {
		int_or_long_number, err := stream.Recv()
		if err == io.EOF {
			if sum == 0 {
				return stream.SendAndClose(&pb.FloatOrLongString{
					Type: &pb.FloatOrLongString_LongStringType{
						LongStringType: &pb.LongString{
							Str: long_string,
						},
					},
				})
			} else {
				log.Printf("Returning mean: %v", float32(sum)/float32(total_numbers))
				return stream.SendAndClose(&pb.FloatOrLongString{
					Type: &pb.FloatOrLongString_FloatNumberType{
						FloatNumberType: &pb.FloatNumber{
							Value: float32(sum) / float32(total_numbers),
						},
					},
				})
			}
		}
		if err != nil {
			return err
		}
		switch u := int_or_long_number.Type.(type) {
		case *pb.FloatOrLongString_FloatNumberType:
			{
				sum += u.FloatNumberType.Value
				total_numbers++
			}
		case *pb.FloatOrLongString_LongStringType:
			{
				long_string = u.LongStringType.Str
			}
		}
	}
}

func (s *server) ComputeMeanRepeatedOrSendLongString(ctx context.Context, in *pb.FloatNumberListOrLongString) (*pb.FloatOrLongString, error) {
	switch u := in.Type.(type) {
	case *pb.FloatNumberListOrLongString_FloatNumberListType:
		{
			var sum float32
			var total_numbers int32
			var input []float32
			input = u.FloatNumberListType.Value
			for _, s := range input {
				sum += float32(s)
				total_numbers++
			}
			// log.Printf("Returning mean: %v", float32(sum)/float32(total_numbers))
			return &pb.FloatOrLongString{
				Type: &pb.FloatOrLongString_FloatNumberType{
					FloatNumberType: &pb.FloatNumber{
						Value: float32(sum) / float32(total_numbers),
					},
				},
			}, nil
		}
	case *pb.FloatNumberListOrLongString_LongStringType:
		{
			var long_string string
			long_string = u.LongStringType.Str
			return &pb.FloatOrLongString{
				Type: &pb.FloatOrLongString_LongStringType{
					LongStringType: &pb.LongString{
						Str: long_string,
					},
				},
			}, nil
		}
	}

	return &pb.FloatOrLongString{
		Type: &pb.FloatOrLongString_LongStringType{
			LongStringType: &pb.LongString{
				Str: "Error",
			},
		},
	}, nil
}

func main() {
	flag.Parse()
	lis, err := net.Listen("tcp", fmt.Sprintf("localhost:%d", *port))
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}

	// set maximum number of concurrent streams in tcp connection
	opts := []grpc.ServerOption{grpc.MaxConcurrentStreams(4)}

	s := grpc.NewServer(opts...)
	pb.RegisterGreeterServer(s, &server{})
	log.Printf("server listening at %v", lis.Addr())
	if err := s.Serve(lis); err != nil {
		log.Fatalf("failed to serve: %v", err)
	}
}

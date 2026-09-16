package oracle

import (
	"context"
	"time"

	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/leaderelection"
	"k8s.io/client-go/tools/leaderelection/resourcelock"
)

type LeaderConfig struct {
	Namespace     string
	LeaseName     string
	Identity      string
	LeaseDuration time.Duration
	RenewDeadline time.Duration
	RetryPeriod   time.Duration
}

func RunAsLeader(ctx context.Context, config LeaderConfig, run func(context.Context)) error {
	restConfig, err := rest.InClusterConfig()
	if err != nil {
		return err
	}
	client, err := kubernetes.NewForConfig(restConfig)
	if err != nil {
		return err
	}
	lock, err := resourcelock.New(
		resourcelock.LeasesResourceLock,
		config.Namespace,
		config.LeaseName,
		client.CoreV1(),
		client.CoordinationV1(),
		resourcelock.ResourceLockConfig{Identity: config.Identity},
	)
	if err != nil {
		return err
	}

	leaderelection.RunOrDie(ctx, leaderelection.LeaderElectionConfig{
		Lock:            lock,
		LeaseDuration:   config.LeaseDuration,
		RenewDeadline:   config.RenewDeadline,
		RetryPeriod:     config.RetryPeriod,
		ReleaseOnCancel: true,
		Name:            config.LeaseName,
		Callbacks: leaderelection.LeaderCallbacks{
			OnStartedLeading: run,
			OnStoppedLeading: func() {},
		},
	})
	return nil
}

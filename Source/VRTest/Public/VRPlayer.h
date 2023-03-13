// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "VRPlayer.generated.h"

UCLASS()
class VRTEST_API AVRPlayer : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AVRPlayer();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	UPROPERTY(VisibleAnywhere, Category = "VRCamera")
	class UCameraComponent* VRCamera;
	UPROPERTY(VisibleAnywhere, Category = "MotionController")
	class UMotionControllerComponent* LeftHand;
	UPROPERTY(VisibleAnywhere, Category = "MotionController")
	class UMotionControllerComponent* RightHand;

public:
	// 블루프린트에서 할당받을 사용자 입력매핑콘텍스트
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputMappingContext* imc_player;
	// 블루프린트에서 할당받을 이동 입력액션
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* ia_move;
	// 이동처리 함수
	void Move(const FInputActionValue& values);

	// 마우스입력액션
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* ia_mouse;
	// 마우스에따른 회전처리 함수
	void Turn(const FInputActionValue& values);

public:	// Teleport
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	class UInputAction* ia_teleport;
	
	/*UPROPERTY(VisibleAnywhere, Category = "MotionController")
	class UStaticMeshComponent* TeleportCircle;*/

	// 텔레포트 기능 활성화 중인지 여부
	bool bTeleporting = false;
	// 텔레포트 활성화 함수
	void TeleportStart(const FInputActionValue& values);
	// 텔레포트 비활성화 함수
	void TeleportEnd(const FInputActionValue& values);

	// 텔레포트 설정 리셋
	bool ResetTeleport();
	// 직선 텔레포트UI 그리기 및 처리함수
	void DrawTeleportStraight();
	// 텔레포트 선과 충돌체크 함수
	bool CheckHitTeleport(FVector LastPos, FVector& CurPos);
	// 충돌처리 유틸리티
	bool HitTest(FVector LastPos, FVector CurPos, FHitResult& HitInfo);

	// 텔레포트 지점
	FVector TeleportLocation;

	// 텔레포트 처리
	void DoStraightTeleport();

private:	// Teleport Curve
	// 텔레포트커브 사용여부
	UPROPERTY(EditAnywhere, Category = "Teleport", Meta = (AllowPrivateAccess = true))
	bool bTeleportCurve = true;
	// 곡선을 이루는 점의 개수(곡선의 부드럽기 정도)
	UPROPERTY(EditAnywhere, Category = "Teleport", Meta = (AllowPrivateAccess = true))
	int lineSmooth = 40;
	// 곡선의 길이가되며 투사체의 발사 거리이기도 하다.
	UPROPERTY(EditAnywhere, Category = "Teleport", Meta = (AllowPrivateAccess = true))
	float curveLength = 1500;
	// 중력 (이 값에따라 곡선의 가파르기가 결정된다.)
	UPROPERTY(EditAnywhere, Category = "Teleport", Meta = (AllowPrivateAccess = true))
	float gravity = -5000;
	// 고정 변위시간
	UPROPERTY(EditAnywhere, Category = "Teleport", Meta = (AllowPrivateAccess = true))
	float simulateTime = 0.02f;
	// 곡선을 이루는 점들 기억
	UPROPERTY()
	TArray<FVector> lines;
	// 곡선텔레포트 시뮬레이션 및 선 그리기
	void DrawTeleportCurve();

protected: // Teleport UI
	UPROPERTY(VisibleAnywhere, Category = "TeleportCurveComp")
	class UNiagaraComponent* TeleportCurveComp;

	//float niagaraTime = .001f;
	//float currentTime = 0;
	// Teleport Circle component
	UPROPERTY(VisibleAnywhere, Category = "TeleportUIComp")
	class UNiagaraComponent* TeleportCircle;

private: // Warp
	// 워프 사용여부
	UPROPERTY(EditAnywhere, Category = "Teleport", Meta = (AllowPrivateAccess = true))
	bool IsWarp = true;

	FTimerHandle WarpHandle;
	float curTime = 0;
	UPROPERTY(EditAnywhere, Category = "Teleport", Meta=(AllowPrivateAccess = true))
	float warpTime = 0.2f;

	void DoWarp();

private: // Fire , crosshair
	UPROPERTY(EditDefaultsOnly, Category = "Input", Meta = (AllowPrivateAccess = true))
	class UInputAction* ia_fire;
	// 사용자 발사 입력처리
	void FireInput(const FInputActionValue& values);

	// 크로스헤어 블루프린트클래스
	UPROPERTY(EditAnywhere, Category = "Crosshair", Meta = (AllowPrivateAccess = true))
	TSubclassOf<AActor> CrosshairFactory;
	// 생성된 크로스헤어 인스턴스
	UPROPERTY()
	AActor* Crosshair;
	// 크로스헤어 그리기
	void DrawCrosshair();

	UPROPERTY(EditDefaultsOnly, Category = "Input", Meta = (AllowPrivateAccess = true))
	class UMotionControllerComponent* RightAim;

private: // Grab
	UPROPERTY(EditDefaultsOnly, Category = "Input", Meta = (AllowPrivateAccess = true))
	class UInputAction* ia_grab;

	UPROPERTY(EditAnywhere, Category = "Grab", Meta = (AllowPrivateAccess = true))
	float grabRadius = 100;

	// 물체 잡고있는지 여부
	bool isGrabbing = false;
	UPROPERTY()
	class UPrimitiveComponent* grabbedObject;

	// 던질 힘
	UPROPERTY(EditAnywhere, Category = "Grab", Meta = (AllowPrivateAccess = true))
	float throwPower = 500;
	// 회전 힘
	UPROPERTY(EditAnywhere, Category = "Grab", Meta = (AllowPrivateAccess = true))
	float toquePower = 1500;

	// 이전위치
	FVector prevPos;
	// 이전회전
	FQuat prevRot;
	// 던질 방향
	FVector throwDirection;
	// 회전할 방향
	FQuat deltaRotation;

	// 물체 잡기
	void TryGrab();
	// 물체 놓기
	void TryUnGrab();
	// 물체 잡은 상태로 컨트롤 하기
	void Grabbing();

public: // Remote Grab
	UPROPERTY(EditDefaultsOnly, Category="Grab")
	bool IsRemoteGrab = true;
	FTimerHandle GrabHandle;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Widget")
	class UWidgetInteractionComponent* WidgetInteractionComp;

	// Release 상태로 되돌려놓기
	void ReleaseUIInput();

public:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	class UInputMappingContext* IMC_Hand;

};

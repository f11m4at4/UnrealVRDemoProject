// Fill out your copyright notice in the Description page of Project Settings.


#include "VRPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MotionControllerComponent.h"
#include "DrawDebugHelpers.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Components/CapsuleComponent.h"
#include "Math/RotationMatrix.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "UMG/Public/Components/WidgetInteractionComponent.h"

#define PRINTToScreen(msg) (GEngine->AddOnScreenDebugMessage(0, 1, FColor::Red, *msg))


// Sets default values
AVRPlayer::AVRPlayer()
{
	PrimaryActorTick.bCanEverTick = true;

	// 카메라컴포넌트 등록
	VRCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VRCamera"));
	VRCamera->SetupAttachment(RootComponent);
	// 모션컨트롤러 등록
	RightHand = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightHand"));
	RightHand->SetupAttachment(RootComponent);
	RightHand->SetTrackingMotionSource(FName("Right"));
	LeftHand = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftHand"));
	LeftHand->SetupAttachment(RootComponent);
	LeftHand->SetTrackingMotionSource(FName("Left"));

	/*TeleportCircle = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TeleportCircle"));
	TeleportCircle->SetupAttachment(RootComponent);
	TeleportCircle->SetRelativeScale3D(FVector(1));
	TeleportCircle->SetCollisionEnabled(ECollisionEnabled::NoCollision);*/

	TeleportCurveComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TeleportCurveComp"));
	TeleportCurveComp->SetupAttachment(RootComponent);
	TeleportCircle = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TeleportUIComp"));
	TeleportCircle->SetupAttachment(RootComponent);

	RightAim = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightAim"));
	RightAim->SetupAttachment(RootComponent);
	RightAim->SetTrackingMotionSource(FName("RightAim"));

	WidgetInteractionComp = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("WidgetInteractionComp"));
	WidgetInteractionComp->SetupAttachment(RightAim);
}

// Called when the game starts or when spawned
void AVRPlayer::BeginPlay()
{
	Super::BeginPlay();
	
	auto pc = Cast<APlayerController>(GetWorld()->GetFirstPlayerController());
	if (pc)
	{
		auto localPlayer = pc->GetLocalPlayer();
		auto subSystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(localPlayer);
		if (subSystem)
		{
			subSystem->AddMappingContext(imc_player, 0);
		}
	}

	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled() == false)
	{
		FVector Pos = RightHand->GetRelativeLocation() + RightHand->GetRightVector() * 20 + RightHand->GetForwardVector() * 100 + RightHand->GetUpVector() * -5;
		RightHand->SetRelativeLocation(Pos);
		RightAim->SetRelativeLocation(Pos);

		// HMD 연결안돼있을 때는 마우스에 따라서 회전하도록 설정
		VRCamera->bUsePawnControlRotation = true;
	}
	else
	{
		UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Eye);
	}
	
	if (CrosshairFactory)
	{
		Crosshair = GetWorld()->SpawnActor(CrosshairFactory);
		//CrosshairOriginScale = Crosshair->GetActorScale();
	}
	ResetTeleport();
}

// Called every frame
void AVRPlayer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// HMD 가 연결돼 있을 때는 VR 컨트롤러를 사용한다.
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled() == false)
	{
		RightHand->SetRelativeRotation(VRCamera->GetRelativeRotation());
		RightAim->SetRelativeRotation(VRCamera->GetRelativeRotation());
	}

	if (bTeleporting)
	{
		if (bTeleportCurve)
		{
			DrawTeleportCurve();
		}
		else
		{
			DrawTeleportStraight();
		}

		// 나이아가라 이용해 텔레포트라인 그리기
		//if (currentTime > niagaraTime)
		if(TeleportCurveComp)
		{
			UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(TeleportCurveComp, FName(TEXT("User.PointArray")), lines);
		}
	}

	DrawCrosshair();

	Grabbing();
}

// Called to bind functionality to input
void AVRPlayer::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	auto inputSystem = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	if (inputSystem)
	{
		//inputSystem->BindAction(ia_move, ETriggerEvent::Triggered, this, FName("Move"));
		inputSystem->BindAction(ia_move, ETriggerEvent::Triggered, this, &AVRPlayer::Move);
		inputSystem->BindAction(ia_mouse, ETriggerEvent::Triggered, this, &AVRPlayer::Turn);
		inputSystem->BindAction(ia_teleport, ETriggerEvent::Started, this, &AVRPlayer::TeleportStart);
		inputSystem->BindAction(ia_teleport, ETriggerEvent::Completed, this, &AVRPlayer::TeleportEnd);

		inputSystem->BindAction(ia_fire, ETriggerEvent::Started, this, &AVRPlayer::FireInput);

		inputSystem->BindAction(ia_grab, ETriggerEvent::Started, this, &AVRPlayer::TryGrab);
		inputSystem->BindAction(ia_grab, ETriggerEvent::Completed, this, &AVRPlayer::TryUnGrab);
	}
}

void AVRPlayer::TeleportStart(const FInputActionValue& values)
{
	bTeleporting = true;
	TeleportCurveComp->SetVisibility(true);
}

void AVRPlayer::TeleportEnd(const FInputActionValue& values)
{
	if (ResetTeleport() == false)
	{
		return;
	}
	
	DoStraightTeleport();

	// 워프실행
	DoWarp();
}


void AVRPlayer::DrawCrosshair()
{
	FHitResult hitInfo;
	// 라인트레스의 시작점
	FVector StartPos = RightAim->GetComponentLocation();
	// 라인트레스의 끝점
	FVector EndPos = StartPos + RightAim->GetForwardVector() * 10000;
	// 충돌체크
	bool bHit = HitTest(StartPos, EndPos, hitInfo);
	
	float distance = 0;
	
	if (bHit)
	{
		// crosshair 작성
		Crosshair->SetActorLocation(hitInfo.ImpactPoint);
		distance = hitInfo.Distance;
	}
	else
	{
		Crosshair->SetActorLocation(EndPos);
		distance = (EndPos - StartPos).Size();
	}
	
	//DrawDebugLine(GetWorld(), StartPos, EndPos, FColor::Yellow, false, -1, 0, 1);
	
	// 크로스헤어의 기본 크기를 CrosshairRatio 배율 만큼 줄여서 표시한다.
	//distance = 1 + distance * CrosshairRatio;

	Crosshair->SetActorScale3D(FVector(FMath::Max<float>(1, distance)));

	// 빌보딩
	FVector Dir = Crosshair->GetActorLocation() - VRCamera->GetComponentLocation();
	Crosshair->SetActorRotation(Dir.Rotation());
}

void AVRPlayer::FireInput(const FInputActionValue& values)
{
	if (WidgetInteractionComp)
	{
		WidgetInteractionComp->PressPointerKey(FKey(TEXT("LeftMouseButton"))); 
	}

	FVector StartPos = RightAim->GetComponentLocation();
	FVector EndPos = StartPos + RightAim->GetForwardVector() * 10000;
	FHitResult hitInfo;
	bool bHit = HitTest(StartPos, EndPos, hitInfo);
	
	if (bHit)
	{
		auto HitComp = hitInfo.GetComponent();
		if (HitComp && HitComp->IsSimulatingPhysics())
		{
			HitComp->AddImpulseAtLocation( (EndPos - StartPos).GetSafeNormal() * 1000, hitInfo.Location);
		}
	}
}

void AVRPlayer::TryGrab()
{
	// 충돌 체크
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredComponent(RightHand);
	TArray<FOverlapResult> HitObjects;
	FVector HandPos = RightHand->GetComponentLocation();
	bool bHit = GetWorld()->OverlapMultiByChannel(HitObjects, HandPos, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(grabRadius), Params);
	if (bHit == false)
	{
		return;
	}

	// 가장 가까운 물체 인덱스
	int closest = 0;
	// 손과 가장 가까운 물체 선택
	for (int i = 0; i < HitObjects.Num(); i++)
	{
		if (HitObjects[i].GetComponent()->IsSimulatingPhysics() == false)
		{
			continue;;
		}
		// 물체 잡고 있는 상태로 설정
		isGrabbing = true;

		// 현재 가장 손과 가까운 위치
		FVector closestPos = HitObjects[closest].GetActor()->GetActorLocation();
		float closestDistance = FVector::Dist(closestPos, HandPos);

		// 다음 물체와 손의 거리
		FVector nextPos = HitObjects[i].GetActor()->GetActorLocation();
		float nextDistance = FVector::Dist(nextPos, HandPos);

		// 다음 물체가 더 손과 가까우면
		if (nextDistance < closestDistance)
		{
			// 가장 가까운 물체 인덱스 교체
			closest = i;
		}
	}
	
	if (isGrabbing)
	{
		// 검출된 물체가 있을 경우 잡도록 설정
		grabbedObject = HitObjects[closest].GetComponent();
		// 붙이기전에 먼저 시뮬레이트 꺼줘야 한다. 안그러면 안붙는다.
		grabbedObject->SetSimulatePhysics(false);
		bool grabResult = grabbedObject->AttachToComponent(RightHand, FAttachmentTransformRules::KeepWorldTransform);
		grabbedObject->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		grabbedObject->IgnoreComponentWhenMoving(GetCapsuleComponent(), true);
		// 초기위치값 지정
		prevPos = RightHand->GetComponentLocation();
		// 초기회전값 지정
		prevRot = RightHand->GetComponentQuat();
		PRINTToScreen((grabResult?FString("Grab True ") : FString("Grab False")));
	}

}

void AVRPlayer::TryUnGrab()
{
	if (isGrabbing == false)
	{
		return;
	}
	// 잡지 않은 상태로 전환
	isGrabbing = false;
	// 물체 손에서 떼어내기
	grabbedObject->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	// 물리기능 활성화
	grabbedObject->SetSimulatePhysics(true);
	// 충돌을 다시 활성화 시켜줘야 물리가 작동한다.
	grabbedObject->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// 던지기
	grabbedObject->AddForce(throwDirection * grabbedObject->GetMass() * throwPower);

	// 회전
	// 각속도 = (1/dt) * dθ(특정 축 기준 변위 각도)
	float angle;
	FVector Axis;
	deltaRotation.ToAxisAndAngle(Axis, angle);
	FVector angularVelocity = (1.0f / GetWorld()->DeltaTimeSeconds) * angle * Axis;
	//grabbedObject->AddTorqueInRadians(angularVelocity * grabbedObject->GetMass() * toquePower);
	grabbedObject->SetPhysicsAngularVelocityInRadians(angularVelocity, true);

	grabbedObject = nullptr;
}

void AVRPlayer::Grabbing()
{
	if (isGrabbing == false)
	{
		return;
	}
	// 던질방향
	throwDirection = RightHand->GetComponentLocation() - prevPos;

	// 쿼터니온 공식
	// angle1 = Q1, angle2 = Q2
	// angle1 + angle2 = Q1 * Q2
	// -angle2 = Quaternion.Inverse(Q2)
	// angle2 - angle1 = Q2 * prevRot.Inverse(Q1)
	deltaRotation = RightHand->GetComponentQuat() * prevRot.Inverse();
	prevPos = RightHand->GetComponentLocation();
	prevRot = RightHand->GetComponentQuat();
}

void AVRPlayer::Move(const FInputActionValue& values)
{
	FVector2D scale = values.Get<FVector2D>();
	AddMovementInput(VRCamera->GetForwardVector(), scale.X);
	AddMovementInput(VRCamera->GetRightVector(), scale.Y);
}

void AVRPlayer::Turn(const FInputActionValue& values)
{
	FVector2D scale = values.Get<FVector2D>();
	AddControllerPitchInput(scale.Y);
	AddControllerYawInput(scale.X);
}

void AVRPlayer::DrawTeleportStraight()
{
	FVector StartPos;
	FVector EndPos;
	
	StartPos = RightHand->GetComponentLocation();
	EndPos = StartPos + RightHand->GetForwardVector() * 1000;
	CheckHitTeleport(StartPos, EndPos);
	lines.RemoveAt(0, lines.Num());
	lines.Add(StartPos);
	lines.Add(EndPos);
	//DrawDebugLine(GetWorld(), StartPos, EndPos, FColor::Red, false, -1, 0, 1);
}

void AVRPlayer::DrawTeleportCurve()
{
	lines.RemoveAt(0, lines.Num());

	// 선이 진행될 방향
	FVector Dir = RightHand->GetForwardVector() * curveLength;
	// 최초 시작위치점
	FVector Pos = RightHand->GetComponentLocation();

	lines.Add(Pos);
	for (int i=0;i<lineSmooth;i++)
	{
		FVector lastPos = Pos;
		// 등가속계산
		Dir += FVector::UpVector * gravity * simulateTime;
		// 등속운동
		Pos += Dir * simulateTime;

		if (CheckHitTeleport(lastPos, Pos))
		{
			lines.Add(Pos);
			break;
		}
		lines.Add(Pos);
	}

	/*for (int i = 0; i < lines.Num() - 1; i++)
	{
		DrawDebugLine(GetWorld(), lines[i], lines[i + 1], FColor::Red, false, -1, 0, 1);
	}*/
}

bool AVRPlayer::ResetTeleport()
{
	// 텔레포트써클 UI 가 활성화 되어 있을 때만 텔레포트가 가능
	bool bCanTeleport = TeleportCircle->GetVisibleFlag();
	// 텔레포트 종료
	bTeleporting = false;
	// 텔레포트 UI 사라지도록처리
	TeleportCircle->SetVisibility(false);
	TeleportCurveComp->SetVisibility(false);
	if (UHeadMountedDisplayFunctionLibrary::IsHeadMountedDisplayEnabled() == false)
	{
		RightHand->SetRelativeRotation(FRotator());
	}

	return bCanTeleport;
}

void AVRPlayer::DoStraightTeleport()
{
	// Warp 옵션 켜져 있으면 처리하지 않는다.
	if (IsWarp)
	{
		return;
	}
	FVector Height = FVector::UpVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	SetActorLocation(TeleportLocation + Height);
}

void AVRPlayer::DoWarp()
{
	// 워프 활성화 되어 있을 때만 실행한다.
	if (!IsWarp)
	{
		return;
	}
	// 정해진 시간(warpTime) 만큼 걸리게 하기 위해 경과시간 curTime 을 설정한다.
	curTime = 0;
	// 워프중에 다른 물체와 충돌하지 않도록 충돌체를 비활성화 시킨다.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 워프는 타이머를 이용해 처리 했다. 0.02(FixedDeltaTime) 에 한번씩 돌도록 처리
	GetWorld()->GetTimerManager().SetTimer(WarpHandle, FTimerDelegate::CreateLambda(
		[this]()->void
		{
			// 1. 시간이 흐른다.
			curTime += GetWorld()->DeltaTimeSeconds;
			FVector CurPos = GetActorLocation();
			// 2. 도착지점은 텔레포트위치에서 캐릭터 피봇위치까지 위로 올린 지점으로 설정한다.
			FVector TargetPos = TeleportLocation + FVector::UpVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			// 3. 러프를 이용해 위치 이동
			CurPos = FMath::Lerp<FVector>(CurPos, TargetPos, curTime / warpTime);
			SetActorLocation(CurPos);
			// 4. 목적지에 도달하면
			if (curTime - warpTime >= 0)
			{
				// 5. 목적지 위치로 설정
				SetActorLocation(TargetPos);
				// 6. 타이머 끄기
				GetWorld()->GetTimerManager().ClearTimer(WarpHandle);
				// 7. 플레이어 충돌 다시 가능하도록 설정
				GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			}
		}
	), 0.02f, true);
}

bool AVRPlayer::CheckHitTeleport(FVector LastPos, FVector& CurPos)
{
	FHitResult HitInfo;
	bool bHit = HitTest(LastPos, CurPos, HitInfo);

	if (bHit && HitInfo.GetActor()->GetName().Contains(TEXT("Floor")))
	{
		// 다음 점의 위치를 충돌한 지점으로 설정
		CurPos = HitInfo.Location;

		// 텔레포트 UI 활성화
		TeleportCircle->SetVisibility(true);
		TeleportCircle->SetWorldLocation(HitInfo.Location);
		TeleportLocation = HitInfo.Location;
	}
	else
	{
		TeleportCircle->SetVisibility(false);
	}

	return bHit;
}

bool AVRPlayer::HitTest(FVector LastPos, FVector CurPos, FHitResult& HitInfo)
{
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	bool bHit = GetWorld()->LineTraceSingleByChannel(HitInfo, LastPos, CurPos, ECC_Visibility, Params);
	return bHit;
}


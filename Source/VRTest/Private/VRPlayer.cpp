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

	// ī�޶�������Ʈ ���
	VRCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VRCamera"));
	VRCamera->SetupAttachment(RootComponent);
	// �����Ʈ�ѷ� ���
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

		// HMD ����ȵ����� ���� ���콺�� ���� ȸ���ϵ��� ����
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

	// HMD �� ����� ���� ���� VR ��Ʈ�ѷ��� ����Ѵ�.
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

		// ���̾ư��� �̿��� �ڷ���Ʈ���� �׸���
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

	// ��������
	DoWarp();
}


void AVRPlayer::DrawCrosshair()
{
	FHitResult hitInfo;
	// ����Ʈ������ ������
	FVector StartPos = RightAim->GetComponentLocation();
	// ����Ʈ������ ����
	FVector EndPos = StartPos + RightAim->GetForwardVector() * 10000;
	// �浹üũ
	bool bHit = HitTest(StartPos, EndPos, hitInfo);
	
	float distance = 0;
	
	if (bHit)
	{
		// crosshair �ۼ�
		Crosshair->SetActorLocation(hitInfo.ImpactPoint);
		distance = hitInfo.Distance;
	}
	else
	{
		Crosshair->SetActorLocation(EndPos);
		distance = (EndPos - StartPos).Size();
	}
	
	//DrawDebugLine(GetWorld(), StartPos, EndPos, FColor::Yellow, false, -1, 0, 1);
	
	// ũ�ν������ �⺻ ũ�⸦ CrosshairRatio ���� ��ŭ �ٿ��� ǥ���Ѵ�.
	//distance = 1 + distance * CrosshairRatio;

	Crosshair->SetActorScale3D(FVector(FMath::Max<float>(1, distance)));

	// ������
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
	// �浹 üũ
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

	// ���� ����� ��ü �ε���
	int closest = 0;
	// �հ� ���� ����� ��ü ����
	for (int i = 0; i < HitObjects.Num(); i++)
	{
		if (HitObjects[i].GetComponent()->IsSimulatingPhysics() == false)
		{
			continue;;
		}
		// ��ü ��� �ִ� ���·� ����
		isGrabbing = true;

		// ���� ���� �հ� ����� ��ġ
		FVector closestPos = HitObjects[closest].GetActor()->GetActorLocation();
		float closestDistance = FVector::Dist(closestPos, HandPos);

		// ���� ��ü�� ���� �Ÿ�
		FVector nextPos = HitObjects[i].GetActor()->GetActorLocation();
		float nextDistance = FVector::Dist(nextPos, HandPos);

		// ���� ��ü�� �� �հ� ������
		if (nextDistance < closestDistance)
		{
			// ���� ����� ��ü �ε��� ��ü
			closest = i;
		}
	}
	
	if (isGrabbing)
	{
		// ����� ��ü�� ���� ��� �⵵�� ����
		grabbedObject = HitObjects[closest].GetComponent();
		// ���̱����� ���� �ùķ���Ʈ ����� �Ѵ�. �ȱ׷��� �Ⱥٴ´�.
		grabbedObject->SetSimulatePhysics(false);
		bool grabResult = grabbedObject->AttachToComponent(RightHand, FAttachmentTransformRules::KeepWorldTransform);
		grabbedObject->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		grabbedObject->IgnoreComponentWhenMoving(GetCapsuleComponent(), true);
		// �ʱ���ġ�� ����
		prevPos = RightHand->GetComponentLocation();
		// �ʱ�ȸ���� ����
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
	// ���� ���� ���·� ��ȯ
	isGrabbing = false;
	// ��ü �տ��� �����
	grabbedObject->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	// ������� Ȱ��ȭ
	grabbedObject->SetSimulatePhysics(true);
	// �浹�� �ٽ� Ȱ��ȭ ������� ������ �۵��Ѵ�.
	grabbedObject->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// ������
	grabbedObject->AddForce(throwDirection * grabbedObject->GetMass() * throwPower);

	// ȸ��
	// ���ӵ� = (1/dt) * d��(Ư�� �� ���� ���� ����)
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
	// ��������
	throwDirection = RightHand->GetComponentLocation() - prevPos;

	// ���ʹϿ� ����
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

	// ���� ����� ����
	FVector Dir = RightHand->GetForwardVector() * curveLength;
	// ���� ������ġ��
	FVector Pos = RightHand->GetComponentLocation();

	lines.Add(Pos);
	for (int i=0;i<lineSmooth;i++)
	{
		FVector lastPos = Pos;
		// ��Ӱ��
		Dir += FVector::UpVector * gravity * simulateTime;
		// ��ӿ
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
	// �ڷ���Ʈ��Ŭ UI �� Ȱ��ȭ �Ǿ� ���� ���� �ڷ���Ʈ�� ����
	bool bCanTeleport = TeleportCircle->GetVisibleFlag();
	// �ڷ���Ʈ ����
	bTeleporting = false;
	// �ڷ���Ʈ UI ���������ó��
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
	// Warp �ɼ� ���� ������ ó������ �ʴ´�.
	if (IsWarp)
	{
		return;
	}
	FVector Height = FVector::UpVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	SetActorLocation(TeleportLocation + Height);
}

void AVRPlayer::DoWarp()
{
	// ���� Ȱ��ȭ �Ǿ� ���� ���� �����Ѵ�.
	if (!IsWarp)
	{
		return;
	}
	// ������ �ð�(warpTime) ��ŭ �ɸ��� �ϱ� ���� ����ð� curTime �� �����Ѵ�.
	curTime = 0;
	// �����߿� �ٸ� ��ü�� �浹���� �ʵ��� �浹ü�� ��Ȱ��ȭ ��Ų��.
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// ������ Ÿ�̸Ӹ� �̿��� ó�� �ߴ�. 0.02(FixedDeltaTime) �� �ѹ��� ������ ó��
	GetWorld()->GetTimerManager().SetTimer(WarpHandle, FTimerDelegate::CreateLambda(
		[this]()->void
		{
			// 1. �ð��� �帥��.
			curTime += GetWorld()->DeltaTimeSeconds;
			FVector CurPos = GetActorLocation();
			// 2. ���������� �ڷ���Ʈ��ġ���� ĳ���� �Ǻ���ġ���� ���� �ø� �������� �����Ѵ�.
			FVector TargetPos = TeleportLocation + FVector::UpVector * GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			// 3. ������ �̿��� ��ġ �̵�
			CurPos = FMath::Lerp<FVector>(CurPos, TargetPos, curTime / warpTime);
			SetActorLocation(CurPos);
			// 4. �������� �����ϸ�
			if (curTime - warpTime >= 0)
			{
				// 5. ������ ��ġ�� ����
				SetActorLocation(TargetPos);
				// 6. Ÿ�̸� ����
				GetWorld()->GetTimerManager().ClearTimer(WarpHandle);
				// 7. �÷��̾� �浹 �ٽ� �����ϵ��� ����
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
		// ���� ���� ��ġ�� �浹�� �������� ����
		CurPos = HitInfo.Location;

		// �ڷ���Ʈ UI Ȱ��ȭ
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


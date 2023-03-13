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
	// �������Ʈ���� �Ҵ���� ����� �Է¸������ؽ�Ʈ
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputMappingContext* imc_player;
	// �������Ʈ���� �Ҵ���� �̵� �Է¾׼�
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* ia_move;
	// �̵�ó�� �Լ�
	void Move(const FInputActionValue& values);

	// ���콺�Է¾׼�
	UPROPERTY(EditDefaultsOnly, Category="Input")
	class UInputAction* ia_mouse;
	// ���콺������ ȸ��ó�� �Լ�
	void Turn(const FInputActionValue& values);

public:	// Teleport
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	class UInputAction* ia_teleport;
	
	/*UPROPERTY(VisibleAnywhere, Category = "MotionController")
	class UStaticMeshComponent* TeleportCircle;*/

	// �ڷ���Ʈ ��� Ȱ��ȭ ������ ����
	bool bTeleporting = false;
	// �ڷ���Ʈ Ȱ��ȭ �Լ�
	void TeleportStart(const FInputActionValue& values);
	// �ڷ���Ʈ ��Ȱ��ȭ �Լ�
	void TeleportEnd(const FInputActionValue& values);

	// �ڷ���Ʈ ���� ����
	bool ResetTeleport();
	// ���� �ڷ���ƮUI �׸��� �� ó���Լ�
	void DrawTeleportStraight();
	// �ڷ���Ʈ ���� �浹üũ �Լ�
	bool CheckHitTeleport(FVector LastPos, FVector& CurPos);
	// �浹ó�� ��ƿ��Ƽ
	bool HitTest(FVector LastPos, FVector CurPos, FHitResult& HitInfo);

	// �ڷ���Ʈ ����
	FVector TeleportLocation;

	// �ڷ���Ʈ ó��
	void DoStraightTeleport();

private:	// Teleport Curve
	// �ڷ���ƮĿ�� ��뿩��
	UPROPERTY(EditAnywhere, Category = "Teleport", Meta = (AllowPrivateAccess = true))
	bool bTeleportCurve = true;
	// ��� �̷�� ���� ����(��� �ε巴�� ����)
	UPROPERTY(EditAnywhere, Category = "Teleport", Meta = (AllowPrivateAccess = true))
	int lineSmooth = 40;
	// ��� ���̰��Ǹ� ����ü�� �߻� �Ÿ��̱⵵ �ϴ�.
	UPROPERTY(EditAnywhere, Category = "Teleport", Meta = (AllowPrivateAccess = true))
	float curveLength = 1500;
	// �߷� (�� �������� ��� ���ĸ��Ⱑ �����ȴ�.)
	UPROPERTY(EditAnywhere, Category = "Teleport", Meta = (AllowPrivateAccess = true))
	float gravity = -5000;
	// ���� �����ð�
	UPROPERTY(EditAnywhere, Category = "Teleport", Meta = (AllowPrivateAccess = true))
	float simulateTime = 0.02f;
	// ��� �̷�� ���� ���
	UPROPERTY()
	TArray<FVector> lines;
	// ��ڷ���Ʈ �ùķ��̼� �� �� �׸���
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
	// ���� ��뿩��
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
	// ����� �߻� �Է�ó��
	void FireInput(const FInputActionValue& values);

	// ũ�ν���� �������ƮŬ����
	UPROPERTY(EditAnywhere, Category = "Crosshair", Meta = (AllowPrivateAccess = true))
	TSubclassOf<AActor> CrosshairFactory;
	// ������ ũ�ν���� �ν��Ͻ�
	UPROPERTY()
	AActor* Crosshair;
	// ũ�ν���� �׸���
	void DrawCrosshair();

	UPROPERTY(EditDefaultsOnly, Category = "Input", Meta = (AllowPrivateAccess = true))
	class UMotionControllerComponent* RightAim;

private: // Grab
	UPROPERTY(EditDefaultsOnly, Category = "Input", Meta = (AllowPrivateAccess = true))
	class UInputAction* ia_grab;

	UPROPERTY(EditAnywhere, Category = "Grab", Meta = (AllowPrivateAccess = true))
	float grabRadius = 100;

	// ��ü ����ִ��� ����
	bool isGrabbing = false;
	UPROPERTY()
	class UPrimitiveComponent* grabbedObject;

	// ���� ��
	UPROPERTY(EditAnywhere, Category = "Grab", Meta = (AllowPrivateAccess = true))
	float throwPower = 500;
	// ȸ�� ��
	UPROPERTY(EditAnywhere, Category = "Grab", Meta = (AllowPrivateAccess = true))
	float toquePower = 1500;

	// ������ġ
	FVector prevPos;
	// ����ȸ��
	FQuat prevRot;
	// ���� ����
	FVector throwDirection;
	// ȸ���� ����
	FQuat deltaRotation;

	// ��ü ���
	void TryGrab();
	// ��ü ����
	void TryUnGrab();
	// ��ü ���� ���·� ��Ʈ�� �ϱ�
	void Grabbing();

public: // Remote Grab
	UPROPERTY(EditDefaultsOnly, Category="Grab")
	bool IsRemoteGrab = true;
	FTimerHandle GrabHandle;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Widget")
	class UWidgetInteractionComponent* WidgetInteractionComp;

	// Release ���·� �ǵ�������
	void ReleaseUIInput();

public:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	class UInputMappingContext* IMC_Hand;

};

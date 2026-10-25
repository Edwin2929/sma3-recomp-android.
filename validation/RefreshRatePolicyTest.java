package org.gbarecomp;

public final class RefreshRatePolicyTest {
    private static int checks;
    private static void check(boolean value, String message) {
        ++checks;
        if (!value) throw new AssertionError(message);
    }
    private static RefreshRatePolicy.Mode mode(int id, int width, int height, float hz) {
        return new RefreshRatePolicy.Mode(id, width, height, hz);
    }
    public static void main(String[] ignored) {
        RefreshRatePolicy.Mode[] phone = {
            mode(1,1440,3120,60),mode(2,1440,3120,120),
            mode(3,1080,2340,60),mode(4,1080,2340,120)
        };
        check(RefreshRatePolicy.selectMode(60,1440,3120,phone)==1,"60 explicitly selects60");
        check(RefreshRatePolicy.selectMode(120,1440,3120,phone)==2,"120 explicitly selects120");
        check(RefreshRatePolicy.selectMode(60,1080,2340,phone)==3,"resolution retained60");
        check(RefreshRatePolicy.selectMode(120,1080,2340,phone)==4,"resolution retained120");
        check(RefreshRatePolicy.selectMode(999,1440,3120,phone)==1,"invalid choice safely uses60");
        check(RefreshRatePolicy.supportsDouble(1440,3120,phone),"120 supported");
        check(!RefreshRatePolicy.supportsDouble(720,1280,phone),"no implicit resolution switch");
        check(RefreshRatePolicy.selectMode(120,720,1280,phone)==0,"unknown mode leaves platform choice");
        RefreshRatePolicy.Mode[] fractional={mode(7,100,200,59.94f),mode(8,100,200,119.88f)};
        check(RefreshRatePolicy.selectMode(60,100,200,fractional)==7,"fractional60 accepted");
        check(RefreshRatePolicy.selectMode(120,100,200,fractional)==8,"fractional120 accepted");
        check(RefreshRatePolicy.selectMode(60,100,200,new RefreshRatePolicy.Mode[]{mode(9,100,200,120)})==9,"integer multiple fallback");
        check(RefreshRatePolicy.selectMode(60,100,200,new RefreshRatePolicy.Mode[]{mode(9,100,200,90)})==0,"90 not forced for60");
        check(RefreshRatePolicy.selectMode(120,100,200,new RefreshRatePolicy.Mode[]{mode(9,100,200,60)})==0,"60 not mistaken for120");
        check(!RefreshRatePolicy.supportsDouble(100,200,new RefreshRatePolicy.Mode[]{mode(9,100,200,60)}),"60-only capability");
        check(RefreshRatePolicy.selectMode(60,100,200,new RefreshRatePolicy.Mode[]{null,mode(1,100,200,Float.NaN),mode(2,100,200,Float.POSITIVE_INFINITY),mode(3,100,200,-60)})==0,"invalid advertised values rejected");
        check(RefreshRatePolicy.selectMode(60,100,200,null)==0,"no modes");
        check(!RefreshRatePolicy.supportsDouble(100,200,null),"no capability");
        for(boolean requested:new boolean[]{false,true}) for(boolean active:new boolean[]{false,true})
            for(boolean surface:new boolean[]{false,true}) {
                check(RefreshRatePolicy.doubleRateActive(active,surface,requested,120,120)==(requested&&active&&surface),"lifecycle gate");
            }
        check(!RefreshRatePolicy.doubleRateActive(true,true,true,120,60),"app override60 blocks extra presents");
        check(!RefreshRatePolicy.doubleRateActive(true,true,true,60,120),"physical60 blocks extra presents");
        check(!RefreshRatePolicy.doubleRateActive(true,true,true,Float.NaN,120),"unknown physical rate");
        check(!RefreshRatePolicy.doubleRateActive(true,true,true,120,Float.NaN),"unknown app rate");
        check(RefreshRatePolicy.doubleRateActive(true,true,true,119.88f,119.88f),"fractional double cadence");
        check(RefreshRatePolicy.GBA_HZ>59.72f && RefreshRatePolicy.GBA_HZ<59.74f,"native guest cadence preserved");
        System.out.println("PASS refresh-rate policy checks="+checks);
    }
}

package eu.unify.doubledecker;

public interface DDEvents {
    public void registered(String endpoint);
    public void disconnected(String endpoint);
    public void publish(String source, String topic, byte[] data);
    public void data(String source, byte[] data);
    public void error(int code, String reason);
}
